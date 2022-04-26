/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/account/confirmations/confirmations.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "base/check_op.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "bat/ads/ad_type.h"
#include "bat/ads/ads_client.h"
#include "bat/ads/confirmation_type.h"
#include "bat/ads/internal/account/account_util.h"
#include "bat/ads/internal/account/confirmations/confirmations_state.h"
#include "bat/ads/internal/account/confirmations/confirmations_user_data_builder.h"
#include "bat/ads/internal/account/issuers/issuers_util.h"
#include "bat/ads/internal/account/redeem_unblinded_token/create_confirmation_util.h"
#include "bat/ads/internal/account/redeem_unblinded_token/redeem_unblinded_token.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/privacy/privacy_util.h"
#include "bat/ads/internal/privacy/tokens/token_generator_interface.h"
#include "bat/ads/internal/privacy/unblinded_payment_tokens/unblinded_payment_token_info.h"
#include "bat/ads/internal/privacy/unblinded_payment_tokens/unblinded_payment_tokens.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_token_info.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/internal/time_formatting_util.h"
#include "bat/ads/pref_names.h"
#include "bat/ads/transaction_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ads {

namespace {
constexpr int64_t kRetryAfterSeconds = 15;
}  // namespace

Confirmations::Confirmations(
    privacy::cbr::TokenGeneratorInterface* token_generator)
    : token_generator_(token_generator),
      redeem_unblinded_token_(std::make_unique<RedeemUnblindedToken>()) {
  DCHECK(token_generator_);

  redeem_unblinded_token_->set_delegate(this);
}

Confirmations::~Confirmations() {
  delegate_ = nullptr;
}

void Confirmations::Confirm(const TransactionInfo& transaction) {
  DCHECK(transaction.IsValid());

  BLOG(1, "Confirming " << transaction.confirmation_type << " for "
                        << transaction.ad_type << " with transaction id "
                        << transaction.id << " and creative instance id "
                        << transaction.creative_instance_id);

  const base::Time now = base::Time::Now();

  const ConfirmationsUserDataBuilder user_data_builder(
      now, transaction.creative_instance_id, transaction.confirmation_type);
  user_data_builder.Build([=](const base::Value& user_data) {
    const ConfirmationInfo& confirmation = CreateConfirmation(
        now, transaction.id, transaction.creative_instance_id,
        transaction.confirmation_type, transaction.ad_type, transaction.value,
        user_data);

    redeem_unblinded_token_->Redeem(confirmation);
  });
}

void Confirmations::ProcessRetryQueue() {
  if (retry_timer_.IsRunning()) {
    return;
  }

  Retry();
}

///////////////////////////////////////////////////////////////////////////////

void Confirmations::Retry() {
  const ConfirmationList& failed_confirmations =
      ConfirmationsState::Get()->GetFailedConfirmations();
  if (failed_confirmations.empty()) {
    BLOG(1, "No failed confirmations to retry");
    return;
  }

  DCHECK(!retry_timer_.IsRunning());
  const base::Time time = retry_timer_.StartWithPrivacy(
      base::Seconds(kRetryAfterSeconds),
      base::BindOnce(&Confirmations::OnRetry, base::Unretained(this)));

  BLOG(1, "Retry sending failed confirmations " << FriendlyDateAndTime(time));
}

void Confirmations::OnRetry() {
  const ConfirmationList& failed_confirmations =
      ConfirmationsState::Get()->GetFailedConfirmations();
  DCHECK(!failed_confirmations.empty());

  const ConfirmationInfo& confirmation = failed_confirmations.front();

  RemoveFromRetryQueue(confirmation);

  redeem_unblinded_token_->Redeem(confirmation);
}

void Confirmations::StopRetrying() {
  retry_timer_.Stop();
}

ConfirmationInfo Confirmations::CreateConfirmation(
    const base::Time time,
    const std::string& transaction_id,
    const std::string& creative_instance_id,
    const ConfirmationType& confirmation_type,
    const AdType& ad_type,
    const double value,
    const base::Value& user_data) const {
  DCHECK(!transaction_id.empty());
  DCHECK(!creative_instance_id.empty());
  DCHECK_NE(ConfirmationType::kUndefined, confirmation_type.value());
  DCHECK_NE(AdType::kUndefined, ad_type.value());

  ConfirmationInfo confirmation;

  confirmation.id = base::GUID::GenerateRandomV4().AsLowercaseString();
  confirmation.transaction_id = transaction_id;
  confirmation.creative_instance_id = creative_instance_id;
  confirmation.type = confirmation_type;
  confirmation.ad_type = ad_type;
  confirmation.value = value;
  confirmation.created_at = time;

  if (ShouldRewardUser() &&
      !ConfirmationsState::Get()->get_unblinded_tokens()->IsEmpty()) {
    const privacy::UnblindedTokenInfo& unblinded_token =
        ConfirmationsState::Get()->get_unblinded_tokens()->GetToken();

    confirmation.unblinded_token = unblinded_token;

    confirmation.tokens = GenerateTokensForValue(value);

    confirmation.blinded_tokens =
        privacy::cbr::BlindTokens(confirmation.tokens);

    std::string json;
    base::JSONWriter::Write(user_data, &json);
    confirmation.user_data = json;

    const std::string& payload = CreateConfirmationRequestDTO(confirmation);
    confirmation.credential = CreateCredential(unblinded_token, payload);

    ConfirmationsState::Get()->get_unblinded_tokens()->RemoveToken(
        unblinded_token);
    ConfirmationsState::Get()->Save();
  }

  return confirmation;
}

privacy::cbr::TokenList Confirmations::GenerateTokensForValue(
    const double value) const {
  int token_count = 1;

  const absl::optional<double> smallest_denomination_optional =
      GetSmallestNonZeroDenominationForIssuerType(IssuerType::kPayments);

  if (value > 0.0 && smallest_denomination_optional) {
    const double smallest_denomination = smallest_denomination_optional.value();

    token_count = static_cast<int>(ceil(value / smallest_denomination));
  }

  return token_generator_->Generate(token_count);
}

void Confirmations::CreateNewConfirmationAndAppendToRetryQueue(
    const ConfirmationInfo& confirmation) {
  DCHECK(confirmation.IsValid());

  if (ConfirmationsState::Get()->get_unblinded_tokens()->IsEmpty()) {
    AppendToRetryQueue(confirmation);
    return;
  }

  const ConfirmationsUserDataBuilder user_data_builder(
      confirmation.created_at, confirmation.creative_instance_id,
      confirmation.type);
  user_data_builder.Build([=](const base::Value& user_data) {
    const ConfirmationInfo& new_confirmation =
        CreateConfirmation(confirmation.created_at, confirmation.transaction_id,
                           confirmation.creative_instance_id, confirmation.type,
                           confirmation.ad_type, confirmation.value, user_data);

    AppendToRetryQueue(new_confirmation);
  });
}

void Confirmations::AppendToRetryQueue(const ConfirmationInfo& confirmation) {
  DCHECK(confirmation.IsValid());

  ConfirmationsState::Get()->AppendFailedConfirmation(confirmation);
  ConfirmationsState::Get()->Save();

  BLOG(1, "Added " << confirmation.type << " confirmation for "
                   << confirmation.ad_type << " with id " << confirmation.id
                   << ", transaction id" << confirmation.transaction_id
                   << " and creative instance id "
                   << confirmation.creative_instance_id
                   << " to the confirmations queue");
}

void Confirmations::RemoveFromRetryQueue(const ConfirmationInfo& confirmation) {
  DCHECK(confirmation.IsValid());

  if (!ConfirmationsState::Get()->RemoveFailedConfirmation(confirmation)) {
    BLOG(0, "Failed to remove " << confirmation.type << " confirmation for "
                                << confirmation.ad_type << " with id "
                                << confirmation.id << ", transaction id "
                                << confirmation.transaction_id
                                << " and creative instance id "
                                << confirmation.creative_instance_id
                                << " from the confirmations queue");

    return;
  }

  BLOG(1, "Removed " << confirmation.type << " confirmation for "
                     << confirmation.ad_type << " with id " << confirmation.id
                     << ", transaction id " << confirmation.transaction_id
                     << " and creative instance id "
                     << confirmation.creative_instance_id
                     << " from the confirmations queue");

  ConfirmationsState::Get()->Save();
}

void Confirmations::OnDidSendConfirmation(
    const ConfirmationInfo& confirmation) {
  BLOG(1, "Successfully sent " << confirmation.type << " confirmation for "
                               << confirmation.ad_type << " with id "
                               << confirmation.id << ", transaction id "
                               << confirmation.transaction_id
                               << " and creative instance id "
                               << confirmation.creative_instance_id);

  if (delegate_) {
    delegate_->OnDidConfirm(confirmation);
  }

  StopRetrying();
  ProcessRetryQueue();
}

void Confirmations::OnFailedToSendConfirmation(
    const ConfirmationInfo& confirmation,
    const bool should_retry) {
  BLOG(1, "Failed to send " << confirmation.type << " confirmation for "
                            << confirmation.ad_type << " with id "
                            << confirmation.id << ", transaction id "
                            << confirmation.transaction_id
                            << " and creative instance id "
                            << confirmation.creative_instance_id);

  if (should_retry) {
    AppendToRetryQueue(confirmation);
  }

  if (delegate_) {
    delegate_->OnFailedToConfirm(confirmation);
  }

  ProcessRetryQueue();
}

void Confirmations::OnDidRedeemUnblindedToken(
    const ConfirmationInfo& confirmation,
    const privacy::UnblindedPaymentTokenInfo& unblinded_payment_token) {
  if (ConfirmationsState::Get()->get_unblinded_payment_tokens()->TokenExists(
          unblinded_payment_token)) {
    BLOG(1, "Unblinded payment token is a duplicate");
    OnFailedToRedeemUnblindedToken(confirmation, /* should_retry */ false);
    return;
  }

  ConfirmationsState::Get()->get_unblinded_payment_tokens()->AddTokens(
      {unblinded_payment_token});
  ConfirmationsState::Get()->Save();

  const int unblinded_payment_tokens_count =
      ConfirmationsState::Get()->get_unblinded_payment_tokens()->Count();

  const base::Time next_token_redemption_at = base::Time::FromDoubleT(
      AdsClientHelper::Get()->GetDoublePref(prefs::kNextTokenRedemptionAt));

  BLOG(1, "Successfully redeemed unblinded token for "
              << confirmation.ad_type << " with confirmation id "
              << confirmation.id << ", transaction id "
              << confirmation.transaction_id << ", creative instance id "
              << confirmation.creative_instance_id << " and "
              << confirmation.type << ". You now have "
              << unblinded_payment_tokens_count
              << " unblinded payment tokens which will be redeemed "
              << FriendlyDateAndTime(next_token_redemption_at));

  if (delegate_) {
    delegate_->OnDidConfirm(confirmation);
  }

  StopRetrying();
  ProcessRetryQueue();
}

void Confirmations::OnFailedToRedeemUnblindedToken(
    const ConfirmationInfo& confirmation,
    const bool should_retry) {
  BLOG(1, "Failed to redeem unblinded token for "
              << confirmation.ad_type << " with confirmation id "
              << confirmation.id << ", transaction id "
              << confirmation.transaction_id << ", creative instance id "
              << confirmation.creative_instance_id << " and "
              << confirmation.type);

  if (should_retry) {
    if (!confirmation.was_created) {
      CreateNewConfirmationAndAppendToRetryQueue(confirmation);
    } else {
      AppendToRetryQueue(confirmation);
    }
  }

  if (delegate_) {
    delegate_->OnFailedToConfirm(confirmation);
  }

  ProcessRetryQueue();
}

void Confirmations::OnIssuersOutOfDate() {
  if (delegate_) {
    delegate_->OnIssuersOutOfDate();
  }
}

}  // namespace ads

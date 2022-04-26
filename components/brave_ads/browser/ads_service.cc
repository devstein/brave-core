/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/browser/ads_service.h"

#include "base/time/time.h"
#include "brave/components/brave_ads/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace brave_ads {

AdsService::AdsService() = default;

AdsService::~AdsService() = default;

void AdsService::AddObserver(AdsServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AdsService::RemoveObserver(AdsServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AdsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAdsWereDisabled, false);
  registry->RegisterBooleanPref(prefs::kHasAdsP3AState, false);

  registry->RegisterBooleanPref(prefs::kShouldShowMyFirstAdNotification, true);

  registry->RegisterIntegerPref(prefs::kSupportedCountryCodesLastSchemaVersion,
                                0);

  registry->RegisterIntegerPref(
      prefs::kSupportedCountryCodesSchemaVersion,
      prefs::kSupportedCountryCodesSchemaVersionNumber);

  registry->RegisterIntegerPref(prefs::kVersion, prefs::kCurrentVersionNumber);

  registry->RegisterBooleanPref(brave_ads::prefs::kEnabled, false);

  registry->RegisterIntegerPref(prefs::kAdNotificationLastScreenPositionX, 0);
  registry->RegisterIntegerPref(prefs::kAdNotificationLastScreenPositionY, 0);
  registry->RegisterBooleanPref(prefs::kAdNotificationDidFallbackToCustom,
                                false);

  registry->RegisterBooleanPref(
      brave_ads::prefs::kShouldAllowConversionTracking, true);

  registry->RegisterInt64Pref(brave_ads::prefs::kAdsPerHour, -1);

  registry->RegisterIntegerPref(brave_ads::prefs::kIdleTimeThreshold, 15);

  registry->RegisterBooleanPref(
      brave_ads::prefs::kShouldAllowAdsSubdivisionTargeting, false);
  registry->RegisterStringPref(brave_ads::prefs::kAdsSubdivisionTargetingCode,
                               "AUTO");
  registry->RegisterStringPref(
      brave_ads::prefs::kAutoDetectedAdsSubdivisionTargetingCode, "");

  registry->RegisterStringPref(brave_ads::prefs::kCatalogId, "");
  registry->RegisterIntegerPref(brave_ads::prefs::kCatalogVersion, 0);
  registry->RegisterInt64Pref(brave_ads::prefs::kCatalogPing, 0);
  registry->RegisterDoublePref(brave_ads::prefs::kCatalogLastUpdated,
                               base::Time().ToDoubleT());

  registry->RegisterIntegerPref(brave_ads::prefs::kIssuerPing, 7200000);

  registry->RegisterStringPref(brave_ads::prefs::kEpsilonGreedyBanditArms, "");
  registry->RegisterStringPref(
      brave_ads::prefs::kEpsilonGreedyBanditEligibleSegments, "");

  registry->RegisterDoublePref(brave_ads::prefs::kNextTokenRedemptionAt,
                               base::Time::Now().ToDoubleT());

  registry->RegisterBooleanPref(brave_ads::prefs::kHasMigratedConversionState,
                                false);
  registry->RegisterBooleanPref(brave_ads::prefs::kHasMigratedRewardsState,
                                false);
}

}  // namespace brave_ads

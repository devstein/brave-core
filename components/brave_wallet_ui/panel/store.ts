/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createStore, applyMiddleware } from 'redux'
import thunk from 'redux-thunk'

import reducers from './reducers'
import walletPanelAsyncHandler from './async/wallet_panel_async_handler'
import walletAsyncHandler from '../common/async/handlers'
import apiProxy from './wallet_panel_api_proxy.js'

const middlewares = [
  thunk,
  walletAsyncHandler,
  walletPanelAsyncHandler
]

const store = createStore(
    reducers,
    applyMiddleware(...middlewares)
)

apiProxy.getInstance().addEthJsonRpcControllerObserver(store)
apiProxy.getInstance().addKeyringControllerObserver(store)
apiProxy.getInstance().addEthTxControllerObserverObserver(store)
apiProxy.getInstance().addBraveWalletServiceObserver(store)

export default store

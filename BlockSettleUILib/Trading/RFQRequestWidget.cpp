/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "RFQRequestWidget.h"

#include <QLineEdit>
#include <QPushButton>

#include "ApplicationSettings.h"
#include "AuthAddressManager.h"
#include "CelerClient.h"
#include "CurrencyPair.h"
#include "DialogManager.h"
#include "NotificationCenter.h"
#include "OrderListModel.h"
#include "OrdersView.h"
#include "QuoteProvider.h"
#include "RFQDialog.h"
#include "RfqStorage.h"
#include "WalletSignerContainer.h"
#include "Wallets/SyncWalletsManager.h"
#include "UtxoReservationManager.h"

#include "bs_proxy_terminal_pb.pb.h"

#include "ui_RFQRequestWidget.h"

namespace  {
   enum class RFQPages : int
   {
      ShieldPage = 0,
      EditableRFQPage
   };
}

RFQRequestWidget::RFQRequestWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::RFQRequestWidget())
{
   rfqStorage_ = std::make_shared<RfqStorage>();

   ui_->setupUi(this);
   ui_->shieldPage->setTabType(QLatin1String("trade"));

   connect(ui_->shieldPage, &RFQShieldPage::requestPrimaryWalletCreation, this, &RFQRequestWidget::requestPrimaryWalletCreation);

   ui_->pageRFQTicket->setSubmitRFQ([this](const bs::network::RFQ& rfq, bs::UtxoReservationToken utxoRes) {
      onRFQSubmit(rfq, std::move(utxoRes));
   });

   ui_->shieldPage->showShieldLoginToSubmitRequired();

   ui_->pageRFQTicket->lineEditAmount()->installEventFilter(this);
   popShield();
}

RFQRequestWidget::~RFQRequestWidget() = default;

void RFQRequestWidget::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if (walletsManager_ == nullptr) {
      walletsManager_ = walletsManager;
      ui_->pageRFQTicket->setWalletsManager(walletsManager);
      ui_->shieldPage->init(walletsManager, authAddressManager_);

      // Do not listen for walletChanged (too verbose and resets UI too often) and walletsReady (to late and resets UI after startup unexpectedly)
      connect(walletsManager_.get(), &bs::sync::WalletsManager::CCLeafCreated, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::AuthLeafCreated, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &RFQRequestWidget::forceCheckCondition);
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletPromotedToPrimary, this, &RFQRequestWidget::forceCheckCondition);
   }
}

void RFQRequestWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {
      case ShortcutType::Alt_1 : {
         ui_->widgetMarketData->view()->activate();
      }
         break;

      case ShortcutType::Alt_2 : {
         if (ui_->pageRFQTicket->lineEditAmount()->isVisible()) {
            ui_->pageRFQTicket->lineEditAmount()->setFocus();
         }
         else {
            ui_->pageRFQTicket->setFocus();
         }
      }
         break;

      case ShortcutType::Alt_3 : {
         ui_->treeViewOrders->activate();
      }
         break;

      case ShortcutType::Ctrl_S : {
         if (ui_->pageRFQTicket->submitButton()->isEnabled()) {
            ui_->pageRFQTicket->submitButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_S : {
         if (ui_->pageRFQTicket->isEnabled()) {
            ui_->pageRFQTicket->sellButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_B : {
         if (ui_->pageRFQTicket->isEnabled()) {
            ui_->pageRFQTicket->buyButton()->click();
         }
      }
         break;

      case ShortcutType::Alt_P : {
         if (ui_->pageRFQTicket->isEnabled()) {
            if (ui_->pageRFQTicket->numCcyButton()->isChecked()) {
               ui_->pageRFQTicket->denomCcyButton()->click();
            }
            else {
               ui_->pageRFQTicket->numCcyButton()->click();
            }
         }
      }
         break;

      default :
         break;
   }
}

void RFQRequestWidget::setAuthorized(bool authorized)
{
   ui_->widgetMarketData->setAuthorized(authorized);
}

void RFQRequestWidget::hideEvent(QHideEvent* event)
{
   ui_->pageRFQTicket->onParentAboutToHide();
   QWidget::hideEvent(event);
}

bool RFQRequestWidget::eventFilter(QObject* sender, QEvent* event)
{
   if (QEvent::KeyPress == event->type() && ui_->pageRFQTicket->lineEditAmount() == sender) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
      if (Qt::Key_Up == keyEvent->key() || Qt::Key_Down == keyEvent->key()) {
         QKeyEvent *pEvent = new QKeyEvent(QEvent::KeyPress, keyEvent->key(), keyEvent->modifiers());
         QCoreApplication::postEvent(ui_->widgetMarketData->view(), pEvent);
         return true;
      }
   }

   return false;
}

void RFQRequestWidget::showEditableRFQPage()
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   ui_->pageRFQTicket->enablePanel();
   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::EditableRFQPage));
}

void RFQRequestWidget::popShield()
{
   ui_->stackedWidgetRFQ->setEnabled(true);

   ui_->stackedWidgetRFQ->setCurrentIndex(static_cast<int>(RFQPages::ShieldPage));
   ui_->pageRFQTicket->disablePanel();
   ui_->widgetMarketData->view()->setFocus();
}

void RFQRequestWidget::initWidgets(const std::shared_ptr<MarketDataProvider>& mdProvider
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<ApplicationSettings> &appSettings)
{
   appSettings_ = appSettings;
   ui_->widgetMarketData->init(appSettings, ApplicationSettings::Filter_MD_RFQ
      , mdProvider, mdCallbacks);
}

void RFQRequestWidget::init(std::shared_ptr<spdlog::logger> logger
   , const std::shared_ptr<BaseCelerClient>& celerClient
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , std::shared_ptr<QuoteProvider> quoteProvider
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<DialogManager> &dialogManager
   , const std::shared_ptr<WalletSignerContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , OrderListModel *orderListModel
)
{
   logger_ = logger;
   celerClient_ = celerClient;
   authAddressManager_ = authAddressManager;
   quoteProvider_ = quoteProvider;
   assetManager_ = assetManager;
   dialogManager_ = dialogManager;
   signingContainer_ = container;
   armory_ = armory;
   connectionManager_ = connectionManager;
   utxoReservationManager_ = utxoReservationManager;

   ui_->pageRFQTicket->init(logger, authAddressManager, assetManager,
      quoteProvider, container, armory, utxoReservationManager);

   ui_->treeViewOrders->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewOrders->setModel(orderListModel);
   ui_->treeViewOrders->initWithModel(orderListModel);
   connect(quoteProvider_.get(), &QuoteProvider::quoteOrderFilled, [](const std::string &quoteId) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder, {true, QString::fromStdString(quoteId)});
   });
   connect(quoteProvider_.get(), &QuoteProvider::orderFailed, [](const std::string &quoteId, const std::string &reason) {
      NotificationCenter::notify(bs::ui::NotifyType::CelerOrder
         , { false, QString::fromStdString(quoteId), QString::fromStdString(reason) });
   });

   connect(celerClient_.get(), &BaseCelerClient::OnConnectedToServer, this, &RFQRequestWidget::onConnectedToCeler);
   connect(celerClient_.get(), &BaseCelerClient::OnConnectionClosed, this, &RFQRequestWidget::onDisconnectedFromCeler);

   ui_->pageRFQTicket->disablePanel();

   connect(authAddressManager_.get(), &AuthAddressManager::VerifiedAddressListUpdated, this, &RFQRequestWidget::forceCheckCondition);
}

void RFQRequestWidget::onConnectedToCeler()
{
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::CurrencySelected,
                                          this, &RFQRequestWidget::onCurrencySelected));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::BidClicked,
                                          this, &RFQRequestWidget::onBidClicked));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::AskClicked,
                                          this, &RFQRequestWidget::onAskClicked));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::MDHeaderClicked,
                                          this, &RFQRequestWidget::onDisableSelectedInfo));
   marketDataConnection.push_back(connect(ui_->widgetMarketData, &MarketDataWidget::clicked,
                                          this, &RFQRequestWidget::onRefreshFocus));

   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onDisconnectedFromCeler()
{
   for (QMetaObject::Connection &conn : marketDataConnection) {
      QObject::disconnect(conn);
   }

   ui_->shieldPage->showShieldLoginToSubmitRequired();
   popShield();
}

void RFQRequestWidget::onRFQSubmit(const bs::network::RFQ& rfq, bs::UtxoReservationToken ccUtxoRes)
{
   auto authAddr = ui_->pageRFQTicket->selectedAuthAddress();

   auto xbtWallet = ui_->pageRFQTicket->xbtWallet();
   auto fixedXbtInputs = ui_->pageRFQTicket->fixedXbtInputs();

   RFQDialog* dialog = new RFQDialog(logger_, rfq, quoteProvider_
      , authAddressManager_, assetManager_, walletsManager_, signingContainer_, armory_, celerClient_, appSettings_
      , connectionManager_, rfqStorage_, xbtWallet, ui_->pageRFQTicket->recvXbtAddressIfSet(), authAddr, utxoReservationManager_
      , fixedXbtInputs.inputs, std::move(fixedXbtInputs.utxoRes), std::move(ccUtxoRes), this);

   connect(this, &RFQRequestWidget::unsignedPayinRequested, dialog, &RFQDialog::onUnsignedPayinRequested);
   connect(this, &RFQRequestWidget::signedPayoutRequested, dialog, &RFQDialog::onSignedPayoutRequested);
   connect(this, &RFQRequestWidget::signedPayinRequested, dialog, &RFQDialog::onSignedPayinRequested);

   dialog->setAttribute(Qt::WA_DeleteOnClose);

   dialogManager_->adjustDialogPosition(dialog);
   dialog->show();

   ui_->pageRFQTicket->resetTicket();

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();
   ui_->pageRFQTicket->SetProductAndSide(currentInfo.productGroup_, currentInfo.currencyPair_,
                                         currentInfo.bidPrice_, currentInfo.offerPrice_, bs::network::Side::Undefined);
}

bool RFQRequestWidget::checkConditions(const MarketSelectedInfo& selectedInfo)
{
   ui_->stackedWidgetRFQ->setEnabled(true);
   using UserType = CelerClient::CelerUserType;
   const UserType userType = celerClient_->celerUserType();

   using GroupType = RFQShieldPage::ProductType;
   const GroupType group = RFQShieldPage::getProductGroup(selectedInfo.productGroup_);

   switch (userType) {
   case UserType::Market: {
      if (group == GroupType::SpotFX || group == GroupType::SpotXBT) {
         ui_->shieldPage->showShieldReservedTradingParticipant();
         popShield();
         return false;
      } else if (checkWalletSettings(group, selectedInfo)) {
         return false;
      }
      break;
   }
   case UserType::Dealing:
   case UserType::Trading: {
      if ((group == GroupType::SpotXBT || group == GroupType::PrivateMarket) &&
         checkWalletSettings(group, selectedInfo)) {
         return false;
      }
      break;
   }
   default: {
      break;
   }
   }

   if (ui_->stackedWidgetRFQ->currentIndex() != static_cast<int>(RFQPages::EditableRFQPage)) {
      showEditableRFQPage();
   }

   return true;
}

bool RFQRequestWidget::checkWalletSettings(bs::network::Asset::Type productType, const MarketSelectedInfo& selectedInfo)
{
   const CurrencyPair cp(selectedInfo.currencyPair_.toStdString());
   const QString currentProduct = QString::fromStdString(cp.NumCurrency());
   if (ui_->shieldPage->checkWalletSettings(productType, currentProduct)) {
      popShield();
      return true;
   }

   return false;
}

void RFQRequestWidget::forceCheckCondition()
{
   if (!ui_->widgetMarketData) {
      return;
   }

   const auto& currentInfo = ui_->widgetMarketData->getCurrentlySelectedInfo();

   if (!currentInfo.isValid()) {
      return;
   }

   onCurrencySelected(currentInfo);
}

void RFQRequestWidget::onCurrencySelected(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecurityId(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                     selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onBidClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecuritySell(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                       selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onAskClicked(const MarketSelectedInfo& selectedInfo)
{
   if (!checkConditions(selectedInfo)) {
      return;
   }

   ui_->pageRFQTicket->setSecurityBuy(selectedInfo.productGroup_, selectedInfo.currencyPair_,
                                       selectedInfo.bidPrice_, selectedInfo.offerPrice_);
}

void RFQRequestWidget::onDisableSelectedInfo()
{
   ui_->shieldPage->showShieldSelectTargetTrade();
   popShield();
}

void RFQRequestWidget::onRefreshFocus()
{
   if (ui_->stackedWidgetRFQ->currentIndex() == static_cast<int>(RFQPages::EditableRFQPage)) {
      ui_->pageRFQTicket->lineEditAmount()->setFocus();
   }
}

void RFQRequestWidget::onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
{
   switch (response.data_case()) {
      case Blocksettle::Communication::ProxyTerminalPb::Response::kSendUnsignedPayin: {
         const auto &command = response.send_unsigned_payin();
         emit unsignedPayinRequested(command.settlement_id());
         break;
      }

      case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayout: {
         const auto &command = response.sign_payout();
         auto timestamp = QDateTime::fromMSecsSinceEpoch(command.timestamp_ms());
         // payin_data - payin hash . binary
         emit signedPayoutRequested(command.settlement_id(), BinaryData::fromString(command.payin_data()), timestamp);
         break;
      }

      case Blocksettle::Communication::ProxyTerminalPb::Response::kSignPayin: {
         const auto &command = response.sign_payin();
         auto timestamp = QDateTime::fromMSecsSinceEpoch(command.timestamp_ms());
         // unsigned_payin_data - serialized payin. binary
         emit signedPayinRequested(command.settlement_id(), BinaryData::fromString(command.unsigned_payin_data()), timestamp);
         break;
      }

      default:
         break;
   }

   // if not processed - not RFQ releated message. not error
}

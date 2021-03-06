/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __RFQ_REQUEST_WIDGET_H__
#define __RFQ_REQUEST_WIDGET_H__

#include <QWidget>
#include <QTimer>
#include <memory>

#include "CommonTypes.h"
#include "MarketDataWidget.h"
#include "TabWithShortcut.h"
#include "UtxoReservationToken.h"

namespace Ui {
    class RFQRequestWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
   class UTXOReservationManager;
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class BaseCelerClient;
class ConnectionManager;
class DialogManager;
class MarketDataProvider;
class MDCallbacksQt;
class OrderListModel;
class QuoteProvider;
class RfqStorage;
class WalletSignerContainer;

class RFQRequestWidget : public TabWithShortcut
{
Q_OBJECT

public:
   RFQRequestWidget(QWidget* parent = nullptr);
   ~RFQRequestWidget() override;

   void initWidgets(const std::shared_ptr<MarketDataProvider> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<ApplicationSettings> &);

   void init(std::shared_ptr<spdlog::logger> logger
         , const std::shared_ptr<BaseCelerClient>& celerClient
         , const std::shared_ptr<AuthAddressManager> &
         , std::shared_ptr<QuoteProvider> quoteProvider
         , const std::shared_ptr<AssetManager>& assetManager
         , const std::shared_ptr<DialogManager> &dialogManager
         , const std::shared_ptr<WalletSignerContainer> &
         , const std::shared_ptr<ArmoryConnection> &
         , const std::shared_ptr<ConnectionManager> &connectionManager
         , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
         , OrderListModel *orderListModel);

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

   void shortcutActivated(ShortcutType s) override;

   void setAuthorized(bool authorized);

protected:
   void hideEvent(QHideEvent* event) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

signals:
   void requestPrimaryWalletCreation();

   void sendUnsignedPayinToPB(const std::string& settlementId, const bs::network::UnsignedPayinData& unsignedPayinData);
   void sendSignedPayinToPB(const std::string& settlementId, const BinaryData& signedPayin);
   void sendSignedPayoutToPB(const std::string& settlementId, const BinaryData& signedPayout);

   void cancelXBTTrade(const std::string& settlementId);
   void cancelCCTrade(const std::string& orderId);

   void unsignedPayinRequested(const std::string& settlementId);
   void signedPayoutRequested(const std::string& settlementId, const BinaryData& payinHash, QDateTime timestamp);
   void signedPayinRequested(const std::string& settlementId, const BinaryData& unsignedPayin, QDateTime timestamp);

private:
   void showEditableRFQPage();
   void popShield();

   bool checkConditions(const MarketSelectedInfo& productGroup);
   bool checkWalletSettings(bs::network::Asset::Type productType, const MarketSelectedInfo& productGroup);
   void onRFQSubmit(const bs::network::RFQ& rfq, bs::UtxoReservationToken ccUtxoRes);

public slots:
   void onCurrencySelected(const MarketSelectedInfo& selectedInfo);
   void onBidClicked(const MarketSelectedInfo& selectedInfo);
   void onAskClicked(const MarketSelectedInfo& selectedInfo);
   void onDisableSelectedInfo();
   void onRefreshFocus();

   void onMessageFromPB(const Blocksettle::Communication::ProxyTerminalPb::Response &response);

private slots:
   void onConnectedToCeler();
   void onDisconnectedFromCeler();

public slots:
   void forceCheckCondition();

private:
   std::unique_ptr<Ui::RFQRequestWidget> ui_;

   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<BaseCelerClient>        celerClient_;
   std::shared_ptr<QuoteProvider>      quoteProvider_;
   std::shared_ptr<AssetManager>       assetManager_;
   std::shared_ptr<AuthAddressManager> authAddressManager_;
   std::shared_ptr<DialogManager>      dialogManager_;

   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<WalletSignerContainer>    signingContainer_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<ApplicationSettings> appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<bs::UTXOReservationManager> utxoReservationManager_;

   std::shared_ptr<RfqStorage> rfqStorage_;

   QList<QMetaObject::Connection>   marketDataConnection;
};

#endif // __RFQ_REQUEST_WIDGET_H__

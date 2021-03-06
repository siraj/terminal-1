/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef USERSCRIPTRUNNER_H_INCLUDED
#define USERSCRIPTRUNNER_H_INCLUDED

#include <QObject>
#include <QTimer>

#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

#include "UserScript.h"
#include "QuoteProvider.h"

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class MDCallbacksQt;
class SignContainer;
class UserScriptRunner;


//
// UserScriptHandler
//

//! Handler of events in user script.
class UserScriptHandler : public QObject
{
   Q_OBJECT

signals:
   void aqScriptLoaded(const QString &fileName);
   void failedToLoad(const QString &fileName, const QString &error);
   void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
   void sendQuote(const bs::network::QuoteReqNotification &qrn, double price);

public:
   explicit UserScriptHandler(const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<spdlog::logger> &
      , UserScriptRunner *runner, QThread *handlerThread);
   ~UserScriptHandler() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

private slots:
   void onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn);
   void onQuoteReqCancelled(const QString &reqId, bool userCancelled);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqRejected(const QString &reqId, const QString &);
   void initAQ(const QString &fileName);
   void deinitAQ(bool deleteAq = true);
   void onMDUpdate(bs::network::Asset::Type, const QString &security,
      bs::network::MDFields mdFields);
   void onBestQuotePrice(const QString reqId, double price, bool own);
   void onAQReply(const QString &reqId, double price);
   void onAQPull(const QString &reqId);
   void aqTick();

private:
   AutoQuoter *aq_ = nullptr;
   std::shared_ptr<SignContainer>            signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;
   std::shared_ptr<AssetManager> assetManager_;
   std::shared_ptr<spdlog::logger> logger_;

   std::unordered_map<std::string, QObject*> aqObjs_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification> aqQuoteReqs_;
   std::unordered_map<std::string, double>   bestQPrices_;

   struct MDInfo {
      double   bidPrice;
      double   askPrice;
      double   lastPrice;
   };
   std::unordered_map<std::string, MDInfo>  mdInfo_;

   bool aqEnabled_;
   QTimer *aqTimer_;
}; // class UserScriptHandler


//
// UserScriptRunner
//

//! Runner of user script.
class UserScriptRunner : public QObject
{
   Q_OBJECT

signals:
   void initAQ(const QString &fileName);
   void deinitAQ(bool deleteAq);
   void stateChanged(bool enabled);
   void aqScriptLoaded(const QString &fileName);
   void failedToLoad(const QString &fileName, const QString &error);
   void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
   void sendQuote(const bs::network::QuoteReqNotification &qrn, double price);

public:
   UserScriptRunner(const std::shared_ptr<QuoteProvider> &,
      const std::shared_ptr<SignContainer> &,
      const std::shared_ptr<MDCallbacksQt> &,
      const std::shared_ptr<AssetManager> &,
      const std::shared_ptr<spdlog::logger> &,
      QObject *parent);
   ~UserScriptRunner() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

public slots:
   void enableAQ(const QString &fileName);
   void disableAQ();

private:
   QThread *thread_;
   UserScriptHandler *script_;
   std::shared_ptr<spdlog::logger> logger_;
}; // class UserScriptRunner

#endif // USERSCRIPTRUNNER_H_INCLUDED

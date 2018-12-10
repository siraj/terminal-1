#include "SignContainer.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "HDWallet.h"
#include "HeadlessContainer.h"
#include "OfflineSigner.h"

#include <QtcpSocket>
#include <spdlog/spdlog.h>

Q_DECLARE_METATYPE(std::shared_ptr<bs::hd::Wallet>)


SignContainer::SignContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
   : logger_(logger), mode_(opMode)
{
   qRegisterMetaType<std::shared_ptr<bs::hd::Wallet>>();
}


std::shared_ptr<SignContainer> CreateSigner(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &appSettings, SignContainer::OpMode runMode
   , const QString &host, const std::shared_ptr<ConnectionManager>& connectionManager)
{
   const auto &port = appSettings->get<QString>(ApplicationSettings::signerPort);
   const auto &pwHash = appSettings->get<QString>(ApplicationSettings::signerPassword);
   switch (runMode)
   {
   case SignContainer::OpMode::Local:
      if (connectionManager == nullptr) {
         logger->error("[CreateSigner] need connection manager to create local signer");
         return nullptr;
      }
      return std::make_shared<LocalSigner>(logger, appSettings->GetHomeDir()
         , appSettings->get<NetworkType>(ApplicationSettings::netType), port
         , connectionManager, pwHash
         , appSettings->get<double>(ApplicationSettings::autoSignSpendLimit));

   case SignContainer::OpMode::Remote:
      if (connectionManager == nullptr) {
         logger->error("[CreateSigner] need connection manager to create remote signer");
         return nullptr;
      }
      return std::make_shared<RemoteSigner>(logger, host, port, connectionManager, pwHash);

   case SignContainer::OpMode::Offline:
      return std::make_shared<OfflineSigner>(logger, appSettings->get<QString>(ApplicationSettings::signerOfflineDir));

   default:
      logger->error("[CreateSigner] Unknown signer run mode");
      break;
   }
   return nullptr;
}

bool SignerConnectionExists(const QString &host, const QString &port)
{
   QTcpSocket sock;
   sock.connectToHost(host, port.toUInt());
   return sock.waitForConnected(30);
}

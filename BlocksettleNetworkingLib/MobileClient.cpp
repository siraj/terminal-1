#include "MobileClient.h"

#include <spdlog/spdlog.h>
#include <QTimer>
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "ZmqSecuredDataConnection.h"

using namespace AutheID::RP;

namespace
{
   const int kConnectTimeoutSeconds = 10;

   const int kTimeoutSeconds = 120;

   const int kKeySize = 32;

   // Obtained from http://185.213.153.44:8181/key
   const std::string kApiKey = "Pj+Q9SsZloftMkmE7EhA8v2Bz1ZC9aOmUkAKTBW9hagJ";
}

MobileClient::MobileClient(const std::shared_ptr<spdlog::logger> &logger
   , const std::pair<autheid::PrivateKey, autheid::PublicKey> &authKeys
   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , authKeys_(authKeys)
{
   connectionManager_.reset(new ConnectionManager(logger));

   timer_ = new QTimer(this);
   connect(timer_, &QTimer::timeout, this, &MobileClient::timeout);
}

std::string MobileClient::toBase64(const std::string &s)
{
   return QByteArray::fromStdString(s).toBase64().toStdString();
}

std::vector<uint8_t> MobileClient::fromBase64(const std::string &s)
{
   const auto &str = QByteArray::fromBase64(QByteArray::fromStdString(s)).toStdString();
   return std::vector<uint8_t>(str.begin(), str.end());
}

void MobileClient::init(const std::string &serverPubKey
   , const std::string &serverHost, const std::string &serverPort)
{
   serverPubKey_ = serverPubKey;
   serverHost_ = serverHost;
   serverPort_ = serverPort;
}

MobileClient::~MobileClient() = default;

bool MobileClient::sendToAuthServer(const std::string &payload, const AutheID::RP::PayloadType type)
{
   ClientPacket packet;
   packet.set_type(type);
   packet.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());

   const auto signature = autheid::signData(payload, authKeys_.first);

   packet.set_rasign(signature.data(), signature.size());

   packet.set_payload(payload.data(), payload.size());

   return connection_->send(packet.SerializeAsString());
}

bool MobileClient::start(MobileClientRequest requestType, const std::string &email
   , const std::string &walletId, const std::vector<std::string> &knownDeviceIds)
{
   cancel();

   connection_ = connectionManager_->CreateSecuredDataConnection();

   if (!connection_) {
      logger_->error("connection_ == nullptr");
      emit failed(tr("Internal error"));
      return false;
   }

   bool result = connection_->SetServerPublicKey(serverPubKey_);
   if (!result) {
      logger_->error("MobileClient::SetServerPublicKey failed");
      emit failed(tr("Internal error"));
      return false;
   }

   result = connection_->openConnection(serverHost_, serverPort_, this);
   if (!result) {
      logger_->error("MobileClient::openConnection failed");
      emit failed(tr("Internal error"));
      return false;
   }

   isConnecting_ = true;

   email_ = email;
   walletId_ = walletId;

   CreateRequest request;
   request.set_type(RequestDeviceKey);
   request.mutable_devicekey()->set_keyid(walletId_);
   request.set_expiration(kTimeoutSeconds);
   request.set_rapubkey(authKeys_.second.data(), authKeys_.second.size());

   QString action = getMobileClientRequestText(requestType);
   bool newDevice = isMobileClientNewDeviceNeeded(requestType);

   request.set_title(action.toStdString() + " " + walletId);
   request.set_apikey(kApiKey);
   request.set_userid(email_);
   request.mutable_devicekey()->set_usenewdevices(newDevice);

   switch (requestType) {
   case MobileClientRequest::ActivateWallet:
      request.mutable_devicekey()->set_registerkey(RegisterKeyReplace);
      break;
   case MobileClientRequest::DeactivateWallet:
      request.mutable_devicekey()->set_registerkey(RegisterKeyClear);
      break;
   case MobileClientRequest::ActivateWalletNewDevice:
      request.mutable_devicekey()->set_registerkey(RegisterKeyAdd);
      break;
   default:
      request.mutable_devicekey()->set_registerkey(RegisterKeyKeep);
      break;
   }

   for (const std::string& knownDeviceId : knownDeviceIds) {
      request.mutable_devicekey()->add_knowndeviceids(knownDeviceId);
   }

   timer_->start(kConnectTimeoutSeconds * 1000);

   return sendToAuthServer(request.SerializeAsString(), PayloadCreateRequest);
}

void MobileClient::cancel()
{
   isConnecting_ = false;
   timer_->stop();

   if (!connection_ || requestId_.empty()) {
      return;
   }

   connection_->closeConnection();

   CancelRequest request;
   request.set_requestid(requestId_);

   sendToAuthServer(request.SerializeAsString(), PayloadCancelRequest);

   requestId_.clear();
}

// Called from background thread!
void MobileClient::processCreateReply(const uint8_t *payload, size_t payloadSize)
{
   isConnecting_ = false;
   timer_->stop();

   CreateReply reply;
   if (!reply.ParseFromArray(payload, payloadSize)) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(tr("Invalid create reply"));
      return;
   }

   if (!reply.success() || reply.requestid().empty()) {
      logger_->error("Create request failed: {}", reply.errormsg());
      emit failed(tr("Request failed"));
      return;
   }

   requestId_ = reply.requestid();

   ResultRequest request;
   request.set_requestid(requestId_);
   sendToAuthServer(request.SerializeAsString(), PayloadResultRequest);
}

// Called from background thread!
void MobileClient::processResultReply(const uint8_t *payload, size_t payloadSize)
{
   ResultReply reply;
   if (!reply.ParseFromArray(payload, payloadSize)) {
      logger_->error("Can't decode ResultReply packet");
      emit failed(tr("Invalid result reply"));
      return;
   }

   if (reply.requestid() != requestId_) {
      return;
   }

   if (reply.encsecurereply().empty() || reply.deviceid().empty()) {
      emit failed(tr("Cancelled"));
      return;
   }

   autheid::SecureBytes secureReplyData = autheid::decryptData(reply.encsecurereply(), authKeys_.first);
   if (secureReplyData.empty()) {
      emit failed(tr("Decrypt failed"));
      return;
   }

   SecureReply secureReply;
   if (!secureReply.ParseFromArray(secureReplyData.data(), secureReplyData.size())) {
      emit failed(tr("Invalid secure reply"));
      return;
   }

   const std::string &deviceKey = secureReply.devicekey();

   if (deviceKey.size() != kKeySize) {
      emit failed(tr("Invalid key size"));
      return;
   }

   std::string encKey = email_ + SeparatorSymbol + reply.deviceid();

   emit succeeded(encKey, SecureBinaryData(deviceKey));
}

void MobileClient::OnDataReceived(const string &data)
{
   ServerPacket packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("Invalid packet data from AuthServer");
      emit failed(tr("Invalid packet"));
      return;
   }

   if (packet.encpayload().empty()) {
      logger_->error("No payload received from AuthServer");
      emit failed(tr("Missing payload"));
      return;
   }

   const auto &decryptedPayload = autheid::decryptData(packet.encpayload(), authKeys_.first);

   switch (packet.type()) {
   case PayloadCreateReply:
      processCreateReply(decryptedPayload.data(), decryptedPayload.size());
      break;
   case PayloadResultReply:
      processResultReply(decryptedPayload.data(), decryptedPayload.size());
      break;
   case PayloadCancelReply:
      break;
   default:
      logger_->error("Got unknown packet type from AuthServer {}", packet.type());
      emit failed(tr("Unknown packet"));
   }
}

void MobileClient::timeout()
{
   cancel();
   logger_->error("Connection to AuthServer failed, no answer received");
   emit failed(tr("Server offline"));
}

void MobileClient::OnConnected()
{
}

void MobileClient::OnDisconnected()
{
}

void MobileClient::OnError(DataConnectionListener::DataConnectionError errorCode)
{
   emit failed(tr("Connection failed"));
}

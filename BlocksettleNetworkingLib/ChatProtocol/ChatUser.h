#ifndef ChatUser_h__
#define ChatUser_h__

#include <string>
#include <memory>

#include <QObject>

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

namespace Chat
{

   class ChatUser : public QObject
   {
      Q_OBJECT
   public:
      ChatUser(QObject *parent = nullptr);
      std::string userName() const;
      void setUserName(const std::string& displayName);

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

      SecureBinaryData privateKey() const { return privateKey_; }
      void setPrivateKey(SecureBinaryData val) { privateKey_ = val; }

      std::string userEmail() const { return userEmail_; }
      void setUserEmail(std::string val) { userEmail_ = val; }

   signals:
      void userNameChanged(const std::string& displayName);

   private:
      std::string userName_;
      std::string userEmail_;
      BinaryData publicKey_;
      SecureBinaryData privateKey_;
   };

   using ChatUserPtr = std::shared_ptr<ChatUser>;
}

#endif // ChatUser_h__

#include "WalletBackupDialog.h"
#include "ui_WalletBackupDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

#include "HDWallet.h"
#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "PaperBackupWriter.h"
#include "SignContainer.h"
#include "WalletBackupFile.h"
#include "UiUtils.h"


#include <spdlog/spdlog.h>


WalletBackupDialog::WalletBackupDialog(const std::shared_ptr<bs::hd::Wallet> &wallet
   , const std::shared_ptr<SignContainer> &container, QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::WalletBackupDialog)
   , wallet_(wallet)
   , signingContainer_(container)
   , frejaSign_(spdlog::get(""))
   , outputDir_(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString())
{
   ui_->setupUi(this);

   ui_->pushButtonBackup->setEnabled(false);
   ui_->labelFileName->clear();

   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &WalletBackupDialog::reject);
   connect(ui_->pushButtonBackup, &QPushButton::clicked, this, &WalletBackupDialog::accept);
   connect(ui_->pushButtonSelectFile, &QPushButton::clicked, this, &WalletBackupDialog::onSelectFile);
   connect(ui_->radioButtonTextFile, &QRadioButton::clicked, this, &WalletBackupDialog::TextFileClicked);
   connect(ui_->radioButtonPDF, &QRadioButton::clicked, this, &WalletBackupDialog::PDFFileClicked);
   connect(ui_->lineEditPassword, &QLineEdit::textEdited, this, &WalletBackupDialog::onPasswordChanged);
   connect(ui_->lineEditPassword, &QLineEdit::editingFinished, this, &WalletBackupDialog::onPasswordChanged);

   if (signingContainer_ && !signingContainer_->isOffline()) {
      connect(signingContainer_.get(), &SignContainer::DecryptedRootKey, this, &WalletBackupDialog::onRootKeyReceived);
      connect(signingContainer_.get(), &SignContainer::HDWalletInfo, this, &WalletBackupDialog::onHDWalletInfo);
      connect(signingContainer_.get(), &SignContainer::Error, this, &WalletBackupDialog::onContainerError);

      infoReqId_ = signingContainer_->GetInfo(wallet_);
   }

   connect(ui_->pushButtonFreja, &QPushButton::clicked, this, &WalletBackupDialog::startFrejaSign);
   connect(&frejaSign_, &FrejaSignWallet::succeeded, this, &WalletBackupDialog::onFrejaSucceeded);
   connect(&frejaSign_, &FrejaSign::failed, this, &WalletBackupDialog::onFrejaFailed);
   connect(&frejaSign_, &FrejaSignWallet::statusUpdated, this, &WalletBackupDialog::onFrejaStatusUpdated);

   outputFile_ = outputDir_ + "/backup_wallet_" + wallet->getName() + "_" + wallet->getWalletId();
   TextFileClicked();
}

bool WalletBackupDialog::isDigitalBackup() const
{
   return ui_->radioButtonTextFile->isChecked();
}

QString WalletBackupDialog::filePath() const
{
   const std::string ext = isDigitalBackup() ? ".wdb" : ".pdf";
   return selectedFile_.isEmpty() ? QString::fromStdString(outputFile_ + ext) : selectedFile_;
}

void WalletBackupDialog::onRootKeyReceived(unsigned int id, const SecureBinaryData &privKey
   , const SecureBinaryData &chainCode, std::string walletId)
{
   if (!privKeyReqId_ || (id != privKeyReqId_)) {
      return;
   }
   privKeyReqId_ = 0;
   EasyCoDec::Data easyData, edChainCode;
   try {
      easyData = bs::wallet::Seed(NetworkType::Invalid, privKey).toEasyCodeChecksum();
      if (!chainCode.isNull()) {
         edChainCode = bs::wallet::Seed(NetworkType::Invalid, chainCode).toEasyCodeChecksum();
      }
   }
   catch (const std::exception &e) {
      showError(tr("Failed to encode private key"), QLatin1String(e.what()));
      reject();
      return;
   }

   QFile f(filePath());
   if (f.exists()) {
      MessageBoxQuestion qRewrite(tr("Wallet Backup"), tr("Backup already exists")
         , tr("File %1 already exists. Do you want to overwrite it?").arg(filePath()), this);
      if (qRewrite.exec() != QDialog::Accepted) {
         return;
      }
   }

   if (isDigitalBackup()) {
      if (!f.open(QIODevice::WriteOnly)) {
         showError(tr("Failed to save backup file"), tr("Unable to open digital file %1 for writing").arg(f.fileName()));
         reject();
         return;
      }
      WalletBackupFile backupData(wallet_, easyData, edChainCode);

      f.write(QByteArray::fromStdString(backupData.Serialize()));
      f.close();
   }
   else {
      try {
         WalletBackupPdfWriter pdfWriter(QString::fromStdString(wallet_->getName()),
            QString::fromStdString(wallet_->getWalletId()),
            QString::fromStdString(easyData.part1),
            QString::fromStdString(easyData.part2),
            QPixmap(QLatin1String(":/resources/logo_print-250px-300ppi.png")),
            UiUtils::getQRCode(QString::fromStdString(easyData.part1 + "\n" + easyData.part2)));
         if (!pdfWriter.write(filePath())) {
            throw std::runtime_error("write failure");
         }
      }
      catch (const std::exception &e) {
         showError(tr("Failed to output PDF"), QLatin1String(e.what()));
         reject();
         return;
      }
   }
   QDialog::accept();
}

void WalletBackupDialog::onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType encType
   , const SecureBinaryData &encKey)
{
   if (!infoReqId_ || (id != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;
   walletEncType_ = encType;
   userId_ = QString::fromStdString(encKey.toBinStr());
   ui_->groupBoxPassword->setVisible(encType != bs::wallet::EncryptionType::Unencrypted);
   ui_->widgetPassword->setVisible(encType == bs::wallet::EncryptionType::Password);
   ui_->widgetFreja->setVisible(encType == bs::wallet::EncryptionType::Freja);
   ui_->pushButtonBackup->setEnabled(encType == bs::wallet::EncryptionType::Unencrypted);
}

void WalletBackupDialog::showError(const QString &title, const QString &text)
{
   MessageBoxCritical(title, text).exec();
}

void WalletBackupDialog::onContainerError(unsigned int id, std::string errMsg)
{
   if (id == infoReqId_) {
      infoReqId_ = 0;
      ui_->labelTypeDesc->setText(tr("Wallet info request failed: %1").arg(QString::fromStdString(errMsg)));
   }
   else if (id == privKeyReqId_) {
      privKeyReqId_ = 0;
      showError(tr("Private Key Error"), tr("Failed to get private key from signing process: %1").arg(QString::fromStdString(errMsg)));
      if (walletEncType_ == bs::wallet::EncryptionType::Password) {
         ui_->lineEditPassword->clear();
         onPasswordChanged();
      }
   }
}

void WalletBackupDialog::onPasswordChanged()
{
   walletPassword_ = ui_->lineEditPassword->text().toStdString();
   ui_->pushButtonBackup->setEnabled(!walletPassword_.isNull());
}

void WalletBackupDialog::startFrejaSign()
{
   frejaSign_.start(userId_, tr("Backup wallet %1").arg(QString::fromStdString(wallet_->getName())), wallet_->getWalletId());
   ui_->pushButtonFreja->setEnabled(false);
}

void WalletBackupDialog::onFrejaSucceeded(SecureBinaryData password)
{
   walletPassword_ = password;
   ui_->pushButtonBackup->setEnabled(!walletPassword_.isNull());
}

void WalletBackupDialog::onFrejaFailed(const QString &text)
{
   ui_->labelFreja->setText(tr("Freja sign failed: %1").arg(text));
   ui_->pushButtonFreja->setEnabled(true);
}

void WalletBackupDialog::onFrejaStatusUpdated(const QString &status)
{
   ui_->labelFreja->setText(tr("Freja status: %1").arg(status));
}

void WalletBackupDialog::TextFileClicked()
{
   if (!ui_->radioButtonTextFile->isChecked()) {
      ui_->labelTypeDesc->clear();
   }
   else {
      ui_->labelTypeDesc->setText(tr("Output decrypted private key to a text file"));
      if (selectedFile_.isEmpty()) {
         ui_->labelFileName->setText(QString::fromStdString(outputFile_ + ".wdb"));
      }
   }
}

void WalletBackupDialog::PDFFileClicked()
{
   if (!ui_->radioButtonPDF->isChecked()) {
      ui_->labelTypeDesc->clear();
   }
   else {
      ui_->labelTypeDesc->setText(tr("Output decrypted private key to a PDF file in printable format"));
      if (selectedFile_.isEmpty()) {
         ui_->labelFileName->setText(QString::fromStdString(outputFile_ + ".pdf"));
      }
   }
}

void WalletBackupDialog::onSelectFile()
{
   bool isText = ui_->radioButtonTextFile->isChecked();
   QFileDialog dlg;
   dlg.setFileMode(QFileDialog::AnyFile);
   selectedFile_ = dlg.getSaveFileName(this, tr("Select file for backup"), filePath()
      , isText ? QLatin1String("*.wdb") : QLatin1String("*.pdf"));
   if (!selectedFile_.isEmpty()) {
      QFileInfo fi(selectedFile_);
      outputDir_ = fi.path().toStdString();
      ui_->labelFileName->setText(selectedFile_);
   }
}

void WalletBackupDialog::accept()
{
   privKeyReqId_ = signingContainer_->GetDecryptedRootKey(wallet_, walletPassword_);
}

void WalletBackupDialog::reject()
{
   MessageBoxQuestion confCancel(tr("Warning"), tr("ABORT BACKUP PROCESS?")
      , tr("BlockSettle strongly encourages you to take the necessary precautions to ensure you backup your"
         " private keys. Are you sure wish to abort the process?"), this);
   confCancel.setConfirmButtonText(tr("Yes")).setCancelButtonText(tr("No"));
   if (confCancel.exec() == QDialog::Accepted) {
      QDialog::reject();
   }
}

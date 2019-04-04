import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.1
import Qt.labs.settings 1.0

import com.blocksettle.WalletsProxy 1.0
import com.blocksettle.TXInfo 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.AutheIDClient 1.0
import com.blocksettle.AuthSignWalletObject 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.WalletsViewModel 1.0
import com.blocksettle.NsWallet.namespace 1.0

import "StyledControls"
import "BsStyles"
import "BsControls"
import "BsDialogs"
import "js/helper.js" as JsHelper
import "js/qmlDialogs.js" as QmlDialogs


ApplicationWindow {
    id: mainWindow

    visible: true
    title: qsTr("BlockSettle Signer")
    width: 450
    height: 600
//    minimumWidth: 450
//    minimumHeight: 600

    Component.onCompleted: { hide() }

    background: Rectangle {
        color: BSStyle.backgroundColor
    }
    overlay.modal: Rectangle {
        color: BSStyle.backgroundModalColor
    }
    overlay.modeless: Rectangle {
        color: BSStyle.backgroundModeLessColor
    }

    // attached to use from c++
    function messageBoxCritical(title, text, details) {
        return JsHelper.messageBoxCritical(title, text, details)
    }

    InfoBanner {
        id: ibSuccess
        bgColor: "darkgreen"
    }
    InfoBanner {
        id: ibFailure
        bgColor: "darkred"
    }

    DirSelectionDialog {
        id: ldrWoWalletDirDlg
        title: qsTr("Select watching only wallet target directory")
    }
    DirSelectionDialog {
        id: ldrDirDlg
        title: qsTr("Select directory")
    }

    signal passwordEntered(string walletId, QPasswordData passwordData, bool cancelledByUser)

    function createTxSignDialog(prompt, txInfo, walletInfo) {
        // called from QMLAppObj::requestPassword

        var dlg = Qt.createComponent("BsDialogs/TxSignDialog.qml").createObject(mainWindow)
        dlg.walletInfo = walletInfo
        dlg.prompt = prompt
        dlg.txInfo = txInfo

        dlg.bsAccepted.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, false)
        })
        dlg.bsRejected.connect(function() {
            passwordEntered(walletInfo.walletId, dlg.passwordData, true)
        })
        mainWindow.requestActivate()
        dlg.open()

        dlg.init()
    }

    function raiseWindow() {
        JsHelper.raiseWindow()
    }

    function customDialogRequest(dialogName, data) {
        show()
        var dlg = QmlDialogs.customDialogRequest(dialogName, data)
        mainWindow.width = dlg.width
        mainWindow.height = dlg.height
        mainWindow.title = dlg.title

        dlg.dialogsChainFinished.connect(function(){ hide() })
        dlg.nextChainDialogChangedOverloaded.connect(function(nextDialog){
            mainWindow.width = nextDialog.width
            mainWindow.height = nextDialog.height
        })
    }
}
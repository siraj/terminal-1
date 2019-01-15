import QtQuick 2.9
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.2

import com.blocksettle.QmlPdfBackup 1.0
import com.blocksettle.WalletInfo 1.0
import com.blocksettle.QSeed 1.0
import com.blocksettle.QPasswordData 1.0
import com.blocksettle.QmlFactory 1.0

import "../BsControls"
import "../StyledControls"
import "../js/helper.js" as JsHelper

CustomTitleDialogWindow {
    id: root

    property int curPage: 1
    property bool acceptable: (curPage == 1 || seedMatch)
    property bool seedMatch: false
    property QSeed seed: QSeed{}

    title: curPage == 1 ? qsTr("Save your Root Private Key") : qsTr("Confirm Seed")

    width: curPage == 1 ? mainWindow.width * 0.8 : 400
    height: curPage == 1 ? mainWindow.height * 0.98 : 265

    abortConfirmation: true
    abortBoxType: BSAbortBox.WalletCreation

    onEnterPressed: {
        if (btnContinue.enabled) btnContinue.onClicked()
    }

    onSeedChanged: {
        // need to update object since bindings working only for basic types
        pdf.seed = seed
    }

    BSMessageBox {
        id: error
        type: BSMessageBox.Critical
    }

    Connections {
        target: pdf

        onSaveSucceed: function(path){
            JsHelper.messageBox(BSMessageBox.Success, "Create Wallet", "Paper Root Private Key successfully saved", path)
        }
        onSaveFailed: function(path) {
            JsHelper.messageBox(BSMessageBox.Critical, "Create Wallet", "Failed to save Paper Root Private Key", path)
        }

        onPrintSucceed: {
            //JsHelper.messageBox(BSMessageBox.Success, "Create Wallet", "Wallet pdf seed saved")
        }
        onPrintFailed: {
            JsHelper.messageBox(BSMessageBox.Critical, "Create Wallet", "Failed to print seed", "")
        }
    }

    ColumnLayout {
        spacing: 10
        width: parent.width
        id: mainLayout

        CustomLabel {
            text: qsTr("Printing this sheet protects all previous and future addresses generated by this wallet! \
You can copy the \"root key\" by hand if a working printer is not available. \
Please make sure that all data lines contain 9 columns of 4 characters each.\
\n\nRemember to secure your backup in an offline environment. \
The backup is uncrypted and will allow anyone who holds it to recover the entire walletInfo.")

            Layout.leftMargin: 15
            Layout.rightMargin: 15
            Layout.fillWidth: true
            id: label
            horizontalAlignment: Qt.AlignLeft
            visible: curPage == 1
        }

        ScrollView {
            Layout.alignment: Qt.AlignCenter
            Layout.preferredWidth: mainLayout.width * 0.95
            //            Layout.preferredHeight: root.height - (headerText.height+5) - label.height -
            //                                    rowButtons.height - mainLayout.spacing * 3
            //Layout.preferredHeight: root.height * 0.5
            Layout.fillHeight: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            clip: true
            id: scroll
            contentWidth: width
            contentHeight: pdf.preferedHeight
            visible: curPage == 1

            QmlPdfBackup {
                id: pdf
                anchors.fill: parent;
                seed: root.seed
            }
        }


        CustomLabel {
            text: qsTr("Your seed is important! If you lose your seed, your bitcoin assets will be permanently lost. \
To make sure that you have properly saved your seed, please retype it here.")

            id: labelVerify
            horizontalAlignment: Qt.AlignLeft
            Layout.fillWidth: true
            Layout.leftMargin: 15
            Layout.rightMargin: 15
            visible: curPage == 2
        }
        BSEasyCodeInput {
            id: rootKeyInput
            visible: curPage == 2
            //sectionHeaderVisible: false
            line1LabelTxt: qsTr("Line 1")
            line2LabelTxt: qsTr("Line 2")
            onAcceptableInputChanged: {
                if (acceptableInput) {
                    if ((seed.part1 + "\n" + seed.part2) === privateRootKey) {
                        seedMatch = true
                    }
                    else {
                        seedMatch = false
                    }
                }
                else {
                    seedMatch = false
                }
                seedMatch = true  // !!! ONLY FOR TESTING!!!
            }
        }

    }

    cFooterItem: RowLayout {
        CustomButtonBar {
            Layout.fillWidth: true
            id: rowButtons

            CustomButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Cancel")
                onClicked: {
                    JsHelper.openAbortBox(root, abortBoxType)
                }
            }

            CustomButtonPrimary {
                id: btnContinue
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: qsTr("Continue")
                enabled: acceptable
                onClicked: {
                    if (curPage == 1) {
                        curPage = 2;
                    } else if (curPage == 2) {
                        acceptAnimated();
                    }
                }
            }

            CustomButton {
                id: btnPrint
                anchors.right: btnContinue.left
                anchors.bottom: parent.bottom
                text: qsTr("Print")
                visible: curPage == 1
                onClicked: {
                    pdf.print();
                }
            }

            CustomButton {
                id: btnBack
                anchors.right: btnContinue.left
                anchors.bottom: parent.bottom
                text: qsTr("Back")
                visible: curPage == 2
                onClicked: {
                    curPage = 1
//                    root.height++
//                    root.height--

                }
            }

            CustomButton {
                id: btnSave
                anchors.right: btnPrint.left
                anchors.bottom: parent.bottom
                text: qsTr("Save")
                visible: curPage == 1
                onClicked: {
                    pdf.save();
                }
            }
        }
    }
}

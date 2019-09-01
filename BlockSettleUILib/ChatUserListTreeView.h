#ifndef CHATCLIENTUSERVIEW_H
#define CHATCLIENTUSERVIEW_H

#include <QTreeView>
#include "ChatUsersViewItemStyle.h"
#include "ChatProtocol/ChatClientService.h"

class QLabel;
class QMenu;

class ChatUsersViewItemStyle;
class PartyTreeItem;
class ChatPartiesTreeModel;

class ChatUserListTreeView : public QTreeView
{
   Q_OBJECT

public:
   ChatUserListTreeView(QWidget * parent = nullptr);
   // #new_logic : this should leave in chat widget
   void setActiveChatLabel(QLabel * label);

public slots:
   void onCustomContextMenu(const QPoint &);

signals:
   void partyClicked(const QModelIndex& index);
   void removeFromContacts(const std::string& partyId);
   void acceptFriendRequest(const std::string& partyId);
   void declineFriendRequest(const std::string& partyId);
   void setDisplayName(const std::string& partyId, const std::string& contactName);

protected slots:
   void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

private slots:
   void onClicked(const QModelIndex &);
   void onDoubleClicked(const QModelIndex &);
   void editContact(const QModelIndex& index);
   void onEditContact();
   void onRemoveFromContacts();
   void onAcceptFriendRequest();
   void onDeclineFriendRequest();

private:
   PartyTreeItem* internalPartyTreeItem(const QModelIndex& index);
   const Chat::ClientPartyPtr clientPartyPtrFromAction(const QAction* action);
   const std::string& currentUser() const;

   // #new_logic : this should leave in chat widget
   void updateDependUi(const QModelIndex& index);


private:
   // #new_logic : this should leave in chat widget
   QLabel * label_;
};

#endif // CHATCLIENTUSERVIEW_H

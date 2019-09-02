#ifndef PARTYTREEITEM_H
#define PARTYTREEITEM_H

#include <QVariant>
#include "QList"

#include "../BlocksettleNetworkingLib/ChatProtocol/Party.h"
#include "chat.pb.h"
// Internal enum
namespace UI {
   enum class ElementType
   {
      Root = 0,
      Container,
      Party
   };
}

class PartyTreeItem
{
public:
   PartyTreeItem(const QVariant& data, UI::ElementType modelType, PartyTreeItem* parent = nullptr);
   ~PartyTreeItem();

   PartyTreeItem* child(int number);

   int childCount() const;
   int columnCount() const;

   QVariant data() const;

   bool insertChildren(PartyTreeItem* item);
   PartyTreeItem* parent();
   bool removeChildren(int position, int count);
   void removeAll();
   int childNumber() const;
   bool setData(const QVariant& value);

   UI::ElementType modelType() const;

   void increaseUnreadedCounter(int newMessageCount);
   void decreaseUnreadedCounter(int seenMessageCount);
   bool hasNewMessages() const;

private:
   QList<PartyTreeItem*> childItems_;
   QVariant itemData_;
   PartyTreeItem* parentItem_;
   UI::ElementType modelType_;
   int unreadedCounter_ = 0;
};
#endif // PARTYTREEITEM_H

/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef AUTOSIGNQUOTEWIDGET_H
#define AUTOSIGNQUOTEWIDGET_H

#include "BSErrorCode.h"

#include <QWidget>
#include <memory>

class AutoSignQuoteProvider;

namespace Ui {
class AutoSignQuoteWidget;
}

class AutoSignQuoteWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AutoSignQuoteWidget(QWidget *parent = nullptr);
   ~AutoSignQuoteWidget();

   void init(const std::shared_ptr<AutoSignQuoteProvider> &autoSignQuoteProvider);

public slots:
   void onAutoSignStateChanged();
   void onAutoSignQuoteAvailChanged();

   void onAqScriptLoaded();
   void onAqScriptUnloaded();

private slots:
   void aqFillHistory();
   void aqScriptChanged(int curIndex);

   void onAutoQuoteToggled();
   void onAutoSignToggled();

private:
   QString askForAQScript();
   void validateGUI();

private:
   std::unique_ptr<Ui::AutoSignQuoteWidget>      ui_;
   std::shared_ptr<AutoSignQuoteProvider>        autoSignQuoteProvider_;
};

#endif // AUTOSIGNQUOTEWIDGET_H

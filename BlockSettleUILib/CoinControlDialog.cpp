#include "ui_CoinControlDialog.h"
#include "CoinControlDialog.h"
#include <QPushButton>
#include "SelectedTransactionInputs.h"


CoinControlDialog::CoinControlDialog(const std::shared_ptr<SelectedTransactionInputs> &inputs, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::CoinControlDialog())
 , selectedInputs_(inputs)
{
   ui_->setupUi(this);

   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &CoinControlDialog::onAccepted);
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
   connect(ui_->widgetCoinControl, &CoinControlWidget::coinSelectionChanged, this, &CoinControlDialog::onSelectionChanged);
   ui_->widgetCoinControl->initWidget(inputs);
}

CoinControlDialog::~CoinControlDialog() = default;

void CoinControlDialog::onAccepted()
{
   ui_->widgetCoinControl->applyChanges(selectedInputs_);
   accept();
}

void CoinControlDialog::onSelectionChanged(size_t nbSelected, bool autoSelection)
{
   ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(nbSelected > 0 || autoSelection);
}

std::vector<UTXO> CoinControlDialog::selectedInputs() const
{
   if (selectedInputs_->UseAutoSel()) {
      return {};
   }
   return selectedInputs_->GetSelectedTransactions();
}

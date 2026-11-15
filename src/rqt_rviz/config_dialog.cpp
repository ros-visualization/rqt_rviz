// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Open robotics

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include <rqt_rviz/config_dialog.hpp>

namespace rqt_rviz
{

ConfigDialog::ConfigDialog(QWidget * parent)
: QDialog(parent)
{
  // Window configurations
  this->setWindowTitle(tr("Choose configuration"));
  this->setWindowFlags(
    Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint);

  // File
  const auto file_label = new QLabel("File path");
  file_label->setToolTip("Full path to file");

  file_edit_ = new QLineEdit;
  file_edit_->setMinimumWidth(300);

  const auto browse_button = new QPushButton(tr("Browse"));
  connect(browse_button, SIGNAL(clicked()), this, SLOT(OnBrowse()));

  // Hide menu
  const auto hide_label = new QLabel("Hide menu");
  hide_label->setToolTip("Check to hide RViz's top menu bar");

  hide_box_ = new QCheckBox();

  // Buttons
  const auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  this->connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  this->connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

  // Layout
  const auto main_layout = new QGridLayout();

  main_layout->addWidget(file_label, 0, 0);
  main_layout->addWidget(file_edit_, 0, 1);
  main_layout->addWidget(browse_button, 0, 2);

  main_layout->addWidget(hide_label, 1, 0);
  main_layout->addWidget(hide_box_, 1, 1);
  main_layout->setAlignment(hide_box_, Qt::AlignLeft);

  main_layout->addWidget(buttons);
  main_layout->setColumnStretch(1, 2);

  this->setLayout(main_layout);
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::OnBrowse()
{
  const auto filename = QFileDialog::getOpenFileName(
    nullptr, tr("Choose config file:"), "", tr("Rviz config file (*.rviz)"));

  file_edit_->setText(filename);
}

std::string ConfigDialog::GetFile() const
{
  return file_edit_->text().toStdString();
}

void ConfigDialog::SetFile(const std::string & file)
{
  file_edit_->setText(QString::fromStdString(file));
}

bool ConfigDialog::GetHide() const
{
  return hide_box_->isChecked();
}

void ConfigDialog::SetHide(const bool hide)
{
  hide_box_->setChecked(hide);
}

}  // namespace rqt_rviz

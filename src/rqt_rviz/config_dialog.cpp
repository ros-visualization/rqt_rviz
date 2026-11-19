// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Open robotics

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

#include <tl/expected.hpp>

#include <ament_index_cpp/get_package_prefix.hpp>
#include <ament_index_cpp/get_package_share_path.hpp>
#include <rqt_rviz/config_dialog.hpp>

namespace rqt_rviz
{

tl::expected<QString, std::string> detail::resolveDisplayConfigPath(const QString & path)
{
  // An empty path asks RViz to load its default configuration.
  if (path.isEmpty()) {
    return path;
  }

  constexpr auto prefix = "package://";
  QString resolved_path = path;
  if (path.startsWith(prefix)) {
    const QString package_uri = path.mid(strlen(prefix));
    const int separator = package_uri.indexOf('/');
    if (separator <= 0 || separator == package_uri.size() - 1) {
      return tl::make_unexpected("Package URIs must have the form package://<package>/<file>.");
    }

    const QString package = package_uri.left(separator);
    const QString relative_path = package_uri.mid(separator + 1);
    try {
      const auto package_path = ament_index_cpp::get_package_share_path(package.toStdString());
      return QString((package_path / relative_path.toStdString()).c_str());
    } catch (const ament_index_cpp::PackageNotFoundError &) {
      return tl::make_unexpected("ROS package '" + package.toStdString() + "' was not found.");
    }
  }
  return path;
}

tl::expected<void, std::string> detail::checkDisplayConfig(const QString & resolved_path)
{
  const QFileInfo file_info(resolved_path);
  if (!file_info.exists()) {
    return tl::make_unexpected(
      "Configuration file '" + resolved_path.toStdString() + "' does not exist.");
  }
  if (!file_info.isFile()) {
    return tl::make_unexpected(
      "Configuration path '" + resolved_path.toStdString() + "' is not a file.");
  }
  if (!file_info.isReadable()) {
    return tl::make_unexpected(
      "Configuration file '" + resolved_path.toStdString() + "' is not readable.");
  }
  return {};
}

ConfigDialog::ConfigDialog(QWidget * parent)
: QDialog(parent)
{
  // Window configurations
  this->setWindowTitle(tr("Choose configuration"));
  this->setWindowFlags(
    Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint);

  // File
  const auto file_label = new QLabel(tr("File path"));
  file_label->setToolTip(tr("Full path to file (can contain packges:// prefix)"));

  file_edit_ = new QLineEdit;
  file_edit_->setMinimumWidth(300);

  const auto browse_button = new QPushButton(tr("Browse"));
  connect(browse_button, SIGNAL(clicked()), this, SLOT(OnBrowse()));

  // Hide menu
  const auto hide_label = new QLabel(tr("Hide menu"));
  hide_label->setToolTip(tr("Check to hide RViz's top menu bar"));

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

  // An empty name means the file dialog was cancelled; keep the current path then.
  if (!filename.isEmpty()) {
    file_edit_->setText(filename);
  }
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

void ConfigDialog::accept()
{
  // An empty path selects RViz's default configuration; there is no file to check.
  if (file_edit_->text().isEmpty()) {
    QDialog::accept();
    return;
  }

  const auto maybe_resolved_path = detail::resolveDisplayConfigPath(file_edit_->text());
  if (!maybe_resolved_path.has_value()) {
    QMessageBox box(
      QMessageBox::Icon::Warning,
      "package:// error.", "The ROS package after package:// does not exist. Continue anyways?",
      QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    box.setInformativeText(maybe_resolved_path.error().c_str());
    box.setDefaultButton(QMessageBox::StandardButton::No);
    if (box.exec() == QMessageBox::StandardButton::No) {
      return;
    } else {
      // The user has accepted a non-existing package, so it makes no sense to continue with file
      // validity checks - just assume the file good.
      QDialog::accept();
      return;
    }
  }

  const auto check_result = detail::checkDisplayConfig(*maybe_resolved_path);
  if (!check_result.has_value()) {
    QMessageBox box(
      QMessageBox::Icon::Warning,
      "Display config does not exist",
      "The selected display config does not exist. Continue anyways?",
      QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);
    box.setInformativeText(check_result.error().c_str());
    box.setDefaultButton(QMessageBox::StandardButton::No);
    if (box.exec() == QMessageBox::StandardButton::No) {
      return;
    }
  }

  QDialog::accept();
}

}  // namespace rqt_rviz

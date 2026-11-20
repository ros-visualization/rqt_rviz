// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Open robotics

#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>

#include <string>

#include <tl/expected.hpp>

namespace rqt_rviz
{

namespace detail
{

tl::expected<QString, std::string> resolveDisplayConfigPath(const QString & path);

tl::expected<void, std::string> checkDisplayConfig(const QString & resolved_path);

}  // namespace detail

class ConfigDialog : public QDialog
{
  Q_OBJECT

public:
  /** @brief Constructor. */
  explicit ConfigDialog(QWidget * parent = nullptr);

  /** @brief Destructor. */
  ~ConfigDialog() override;

  /** @brief Populate the file path line edit. */
  void SetFile(const std::string & file);

  /** @brief Get the file path entered by the user. */
  std::string GetFile() const;

  /** @brief Get the hide menu option. */
  bool GetHide() const;

  /** @brief Set the hide menu option. */
  void SetHide(const bool hide);

  void accept() override;

private slots:
  /** @brief Callback when the browse button is pressed. */
  void OnBrowse();

private:
  /** @brief Holds the file path. */
  QLineEdit * file_edit_;

  /** @brief Holds the boolean for whether to hide the menu. */
  QCheckBox * hide_box_;
};

}  // namespace rqt_rviz

// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague
// SPDX-FileCopyrightText: Open robotics

#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>

namespace rqt_rviz
{

class ConfigDialog : public QDialog
{
  Q_OBJECT

public:
  /** @brief Constructor. */
  explicit ConfigDialog(QWidget * parent = nullptr);

  /** @brief Destructor. */
  ~ConfigDialog();

  /** @brief Populate the file path line edit. */
  void SetFile(const std::string& file);

  /** @brief Get the file path entered by the user. */
  std::string GetFile() const;

  /** @brief Get the hide menu option. */
  bool GetHide() const;

  /** @brief Set the hide menu option. */
  void SetHide(const bool hide);

private slots:
  /** @brief Callback when the browse button is pressed. */
  void OnBrowse();

private:
  /** @brief Holds the file path. */
  QLineEdit* file_edit_;

  /** @brief Holds the boolean for whether to hide the menu. */
  QCheckBox* hide_box_;
};

}

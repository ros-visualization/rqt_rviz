// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: Czech Technical University in Prague

#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QWidget>

class MockHandler : public QObject
{
  Q_OBJECT

public:
  QWidget container_;
  QList<QWidget *> widgets_;

public Q_SLOTS:
  void add_widget(QWidget * widget)
  {
    widget->setParent(&container_);
    widgets_.append(widget);
  }

  void remove_widget(QWidget * widget)
  {
    widgets_.removeAll(widget);
    widget->setParent(nullptr);
  }

  void dock_widget(QWidget *) {}
};

class MockSettings : public QObject
{
  Q_OBJECT

public:
  QMap<QString, QVariant> values;

public Q_SLOTS:
  bool contains(const QString & key) const
  {
    return values.contains(key);
  }

  QVariant value(const QString & key, const QVariant & defaultValue = QVariant()) const
  {
    return values.value(key, defaultValue);
  }

  void set_value(const QString & key, const QVariant & value)
  {
    values[key] = value;
  }
};

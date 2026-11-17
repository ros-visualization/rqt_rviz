#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: Czech Technical University in Prague

import os
from pathlib import Path
import sys
import tempfile
import time
import traceback

from python_qt_binding.QtCore import QCoreApplication, QEvent, QEventLoop, QMetaObject, QTimer
from python_qt_binding.QtWidgets import QApplication, QDialog, QDockWidget, QFileDialog
from python_qt_binding.QtWidgets import QLabel, QLineEdit, QMenuBar, QMessageBox, QPushButton
from qt_gui.plugin_manager import PluginManager
from rqt_gui.main import Main


PLUGIN_ID = 'rqt_rviz/RViz'
PACKAGE_URI = 'package://rviz_common/default.rviz'


def write_config(path, background, fixed_frame):
    path.write_text(
        'Visualization Manager:\n'
        '  Class: ""\n'
        '  Displays:\n'
        '    - Class: rviz_default_plugins/Grid\n'
        '      Enabled: true\n'
        '      Name: Grid\n'
        '      Value: true\n'
        '  Enabled: true\n'
        '  Global Options:\n'
        f'    Background Color: {background}\n'
        f'    Fixed Frame: {fixed_frame}\n'
        '    Frame Rate: 30\n'
        '  Name: root\n'
        '  Tools:\n'
        '    - Class: rviz_default_plugins/MoveCamera\n'
        '  Value: true\n'
        '  Views:\n'
        '    Current:\n'
        '      Class: rviz_default_plugins/Orbit\n'
        '      Distance: 10\n'
        '      Name: Current View\n'
        '      Target Frame: <Fixed Frame>\n',
        encoding='utf-8')


class RQtLifecycle:

    def __init__(self, manager, main_window, configs):
        self.manager = manager
        self.main_window = main_window
        self.configs = configs

    def instances(self):
        return {
            info['instance_id'].serial_number: info['handler']
            for info in self.manager._running_plugins.values()
            if info['instance_id'].plugin_id == PLUGIN_ID
        }

    def process_events(self, duration):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            QApplication.processEvents(QEventLoop.ProcessEventsFlag.AllEvents, 50)
            QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
            time.sleep(0.01)

    def wait_until(self, predicate, description, timeout=30):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.process_events(0.05)
            if predicate():
                return
            time.sleep(0.01)
        raise AssertionError(f'Timed out waiting for {description}')

    def dock(self, serial):
        widgets = self.instances()[serial]._widgets
        if len(widgets) != 1:
            raise AssertionError(
                f'RViz instance {serial} has {len(widgets)} widgets instead of one')
        return next(iter(widgets.values()))[0]

    def assert_visible(self, serial, config, hide_menu):
        if not self.main_window.isVisible():
            raise AssertionError('The RQt main window is not visible')
        dock = self.dock(serial)
        frame = dock.widget()
        if not dock.isVisible() or not frame.isVisible():
            raise AssertionError(f'RViz instance {serial} is not visible in RQt')
        if dock.width() < 100 or dock.height() < 100:
            raise AssertionError(f'RViz instance {serial} has no usable rendering area')
        self.assert_static_title(serial)
        menu_bars = frame.findChildren(QMenuBar)
        if len(menu_bars) != 1:
            raise AssertionError(
                f'RViz instance {serial} has {len(menu_bars)} menu bars instead of one')
        if menu_bars[0].isHidden() != hide_menu:
            raise AssertionError(f'RViz instance {serial} has the wrong menu visibility')
        if dock.grab().isNull():
            raise AssertionError(f'RViz instance {serial} could not be painted')

    def assert_static_title(self, serial, expected_title=None):
        label = self.dock(serial).findChild(QLabel, 'title_label')
        if label is None:
            raise AssertionError(f'RViz instance {serial} has no title label')
        title = label.text()
        if expected_title is None:
            expected_title = 'RViz' if serial == 1 else f'RViz ({serial})'
        if title != expected_title:
            raise AssertionError(
                f'RViz instance {serial} has a dynamic title: {title}')

    def load(self, expected_serial, config, hide_menu=False, expected_hide_menu=None):
        arguments = [] if config is None else [f'--display-config={config}']
        if hide_menu:
            arguments.append('--hide-menu')
        if expected_hide_menu is None:
            expected_hide_menu = hide_menu
        self.manager.load_plugin(PLUGIN_ID, None, arguments)
        self.wait_until(
            lambda: expected_serial in self.instances(),
            f'RViz instance {expected_serial} to load')
        self.wait_until(
            lambda: self.dock(expected_serial).isVisible(),
            f'RViz instance {expected_serial} to become visible')
        self.process_events(1.0)
        self.assert_visible(expected_serial, config, expected_hide_menu)
        loaded = 'the default configuration' if config is None else Path(config).name
        print(f'Loaded RViz instance {expected_serial} with {loaded}', flush=True)

    def configure(
            self, serial, expected_config, browse_to=None, new_config=None,
            expected_error=None, expected_title=None):
        errors = []

        def choose_file():
            dialog = QApplication.activeModalWidget()
            try:
                if not isinstance(dialog, QFileDialog):
                    raise AssertionError(f'Expected a file dialog, got {dialog}')
                dialog.selectFile(str(browse_to))
                QMetaObject.invokeMethod(dialog, 'accept')
            except BaseException:
                errors.append(traceback.format_exc())
                if isinstance(dialog, QDialog):
                    dialog.reject()

        def dismiss_error():
            dialog = QApplication.activeModalWidget()
            try:
                if not isinstance(dialog, QMessageBox):
                    raise AssertionError(f'Expected a configuration error, got {dialog}')
                if expected_error not in dialog.text():
                    raise AssertionError(
                        f'Expected error containing {expected_error!r}, got {dialog.text()!r}')
                dialog.reject()
            except BaseException:
                errors.append(traceback.format_exc())
                if isinstance(dialog, QDialog):
                    dialog.reject()

        def interact():
            dialog = QApplication.activeModalWidget()
            try:
                if not isinstance(dialog, QDialog):
                    raise AssertionError(f'Expected a configuration dialog, got {dialog}')
                file_edit = dialog.findChild(QLineEdit)
                if file_edit is None:
                    raise AssertionError('The configuration dialog has no file name editor')
                if file_edit.text() != str(expected_config):
                    raise AssertionError(
                        f'Configuration name changed from {expected_config} to {file_edit.text()}')
                if new_config is not None:
                    file_edit.setText(str(new_config))
                if browse_to is not None:
                    browse_buttons = [
                        button for button in dialog.findChildren(QPushButton)
                        if button.text() == 'Browse'
                    ]
                    if len(browse_buttons) != 1:
                        raise AssertionError(
                            'The configuration dialog has no unique Browse button')
                    QTimer.singleShot(0, choose_file)
                    browse_buttons[0].click()
                if errors:
                    dialog.reject()
                else:
                    if expected_error is not None:
                        QTimer.singleShot(0, dismiss_error)
                        # First accept will show the confirmation dialog
                        dialog.accept()
                        # When the confirmation dialog is rejected, reject this one, too
                        dialog.reject()
                        return
                    dialog.accept()
            except BaseException:
                errors.append(traceback.format_exc())
                if isinstance(dialog, QDialog):
                    dialog.reject()

        self.dock(serial).widget().setWindowModified(False)
        QTimer.singleShot(0, interact)
        self.instances()[serial]._trigger_configuration()
        self.process_events(0.5)
        if errors:
            raise AssertionError(errors[0])
        self.assert_static_title(serial, expected_title)

    def unload(self, serial):
        dock_name = self.dock(serial).objectName()
        instance_id = str(self.instances()[serial].instance_id())
        self.manager.unload_plugin(instance_id)
        self.wait_until(
            lambda: serial not in self.instances(),
            f'RViz instance {serial} to unload')
        self.wait_until(
            lambda: self.main_window.findChild(QDockWidget, dock_name) is None,
            f'RViz dock {dock_name} to be removed')
        self.process_events(0.5)
        print(f'Removed RViz instance {serial}', flush=True)

    def set_title(self, serial, title):
        label = self.dock(serial).findChild(QLabel, 'title_label')
        if label is None:
            raise AssertionError(f'RViz instance {serial} has no title label')
        label.setText(title)

    def run(self):
        self.process_events(0.5)
        if self.instances():
            raise AssertionError('RQt did not start with an empty set of RViz plugins')
        if PLUGIN_ID not in self.manager.get_plugins():
            raise AssertionError(f'{PLUGIN_ID} was not discovered by RQt')

        self.load(1, self.configs[0], hide_menu=True)
        self.load(2, self.configs[1])
        self.assert_visible(1, self.configs[0], hide_menu=True)
        self.assert_visible(2, self.configs[1], hide_menu=False)
        self.process_events(2.0)

        self.unload(1)
        self.assert_visible(2, self.configs[1], hide_menu=False)
        self.process_events(1.0)

        # The command-line configuration takes precedence, while RQt restores the
        # previous menu setting associated with the reused instance serial number.
        self.load(1, self.configs[2], expected_hide_menu=True)
        self.assert_visible(1, self.configs[2], hide_menu=True)
        self.assert_visible(2, self.configs[1], hide_menu=False)

        # Accepting the unchanged dialog must retain the package URI. Selecting
        # another file through the browse dialog must replace it with that path.
        self.configure(1, self.configs[2])
        self.configure(1, self.configs[2])
        self.configure(1, self.configs[2], browse_to=self.configs[0])
        self.assert_visible(1, self.configs[0], hide_menu=True)
        self.configure(1, self.configs[0], new_config=self.configs[2])
        self.set_title(1, title='Test')
        self.configure(1, self.configs[2], expected_title='Test')
        self.set_title(1, 'RViz')

        # Invalid selections are reported and do not replace the current config.
        missing_config = self.configs[0].with_name('missing.rviz')
        self.configure(
            1, self.configs[2], new_config=missing_config,
            expected_error='does not exist')
        self.configure(
            1, self.configs[2], new_config='package://missing_rqt_rviz_package/default.rviz',
            expected_error='does not exist')
        self.configure(1, self.configs[2])
        self.process_events(2.0)

        # Reproduce the standalone plugin workflow: no configuration on the command
        # line, the package URI is only typed into the configuration dialog.
        self.load(3, None)
        self.configure(3, '', new_config=PACKAGE_URI)
        self.configure(3, PACKAGE_URI)
        self.assert_visible(3, None, hide_menu=False)

        # A restarted instance must get the typed package URI back from the settings.
        self.unload(3)
        self.load(3, None)
        self.configure(3, PACKAGE_URI)
        self.configure(3, PACKAGE_URI, new_config=self.configs[0])
        self.configure(3, self.configs[0])
        self.unload(3)

        self.process_events(2.0)
        print('RQt lifecycle checks passed; closing with two active plugins', flush=True)


def run_test():
    previous_config_home = os.environ.get('XDG_CONFIG_HOME')
    with tempfile.TemporaryDirectory(prefix='rqt_rviz_') as temporary_directory:
        temporary_path = Path(temporary_directory)
        os.environ['XDG_CONFIG_HOME'] = str(temporary_path / 'config')
        configs = [
            temporary_path / 'one.rviz',
            temporary_path / 'two.rviz',
            PACKAGE_URI,
        ]
        write_config(configs[0], '96; 32; 32', 'map')
        write_config(configs[1], '32; 64; 96', 'odom')

        failures = []
        original_set_main_window = PluginManager.set_main_window

        def set_main_window(manager, main_window, menu_bar, container_manager):
            original_set_main_window(manager, main_window, menu_bar, container_manager)
            lifecycle = RQtLifecycle(manager, main_window, configs)

            def exercise_running_window():
                try:
                    lifecycle.run()
                except BaseException:
                    failures.append(traceback.format_exc())
                    traceback.print_exc()
                finally:
                    QTimer.singleShot(0, main_window.close)

            QTimer.singleShot(0, exercise_running_window)

        PluginManager.set_main_window = set_main_window
        try:
            exit_code = Main().main(argv=['rqt', '--force-discover', '--clear-config'])
        finally:
            PluginManager.set_main_window = original_set_main_window
            if previous_config_home is None:
                del os.environ['XDG_CONFIG_HOME']
            else:
                os.environ['XDG_CONFIG_HOME'] = previous_config_home

    if failures:
        print(failures[0], file=sys.stderr)
        return 1
    if exit_code != 0:
        print(f'RQt exited with code {exit_code}', file=sys.stderr)
        return exit_code
    print('Real RQt window integration test passed', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(run_test())

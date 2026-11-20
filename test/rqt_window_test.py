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

    def watch_message_boxes(self, errors, unsaved_choice=None):
        """
        Answer the unsaved changes prompt and fail on any other unexpected message box.

        Returns the started timer and a list that records the texts of all handled boxes.
        """
        seen = []

        def check():
            dialog = QApplication.activeModalWidget()
            if not isinstance(dialog, QMessageBox) or dialog.property('rqt_rviz_seen'):
                return
            dialog.setProperty('rqt_rviz_seen', True)
            seen.append(dialog.text())
            if 'unsaved changes' in dialog.text() and unsaved_choice is not None:
                dialog.button(unsaved_choice).click()
                return
            errors.append(f'Unexpected message box: {dialog.text()!r}')
            dialog.reject()

        timer = QTimer()
        timer.setInterval(50)
        timer.timeout.connect(check)
        timer.start()
        return timer, seen

    def frame(self, serial):
        return self.dock(serial).widget()

    def configure(
            self, serial, expected_config, browse_to=None, new_config=None,
            expected_error=None, expected_title=None, browse_cancel=False,
            reject=False, modified=False, unsaved_choice=None):
        errors = []
        watcher = None
        if expected_error is None:
            watcher, seen_boxes = self.watch_message_boxes(errors, unsaved_choice)

        def choose_file():
            dialog = QApplication.activeModalWidget()
            try:
                if not isinstance(dialog, QFileDialog):
                    raise AssertionError(f'Expected a file dialog, got {dialog}')
                if browse_cancel:
                    QMetaObject.invokeMethod(dialog, 'reject')
                    return
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
                if browse_to is not None or browse_cancel:
                    browse_buttons = [
                        button for button in dialog.findChildren(QPushButton)
                        if button.text() == 'Browse'
                    ]
                    if len(browse_buttons) != 1:
                        raise AssertionError(
                            'The configuration dialog has no unique Browse button')
                    QTimer.singleShot(0, choose_file)
                    browse_buttons[0].click()
                    if browse_cancel and file_edit.text() != str(expected_config):
                        raise AssertionError(
                            'Cancelling the file dialog changed the configuration name '
                            f'from {expected_config} to {file_edit.text()!r}')
                if errors or reject:
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

        self.frame(serial).setWindowModified(modified)
        QTimer.singleShot(0, interact)
        self.instances()[serial]._trigger_configuration()
        self.process_events(0.5)
        if watcher is not None:
            watcher.stop()
            if unsaved_choice is not None and not any(
                    'unsaved changes' in text for text in seen_boxes):
                errors.append('The user was not asked about unsaved changes')
        if errors:
            raise AssertionError(errors[0])
        self.assert_static_title(serial, expected_title)

    def loaded_file(self, serial):
        """Return the file RViz has open, as shown in the RViz window title."""
        title = self.frame(serial).windowTitle()
        if title == 'RViz[*]':
            return 'default'
        if not title.endswith('[*] - RViz'):
            raise AssertionError(f'Unexpected RViz window title {title!r}')
        return title[:-len('[*] - RViz')]

    def use_rviz_file_menu(self, serial, action_text, path):
        """Open or save a config through RViz's own File menu."""
        errors = []

        def choose_file():
            dialog = QApplication.activeModalWidget()
            try:
                if not isinstance(dialog, QFileDialog):
                    raise AssertionError(f'Expected a file dialog, got {dialog}')
                dialog.selectFile(str(path))
                QMetaObject.invokeMethod(dialog, 'accept')
            except BaseException:
                errors.append(traceback.format_exc())
                if isinstance(dialog, QDialog):
                    dialog.reject()

        actions = [a for a in self.frame(serial).actions() if a.text() == action_text]
        if len(actions) != 1:
            raise AssertionError(f'RViz has no unique {action_text!r} action')
        self.frame(serial).setWindowModified(False)
        QTimer.singleShot(0, choose_file)
        actions[0].trigger()
        self.process_events(0.5)
        if errors:
            raise AssertionError(errors[0])
        if self.loaded_file(serial) != str(path):
            raise AssertionError(
                f'RViz did not switch to {path}, it shows {self.loaded_file(serial)}')

    def status_text(self, serial):
        return ' '.join(label.text() for label in self.frame(serial).findChildren(QLabel))

    def check_review_fixes(self):
        serial = 3
        one, two = self.configs[0], self.configs[1]

        # A cancelled Browse dialog keeps the current name.
        self.load(serial, one)
        self.configure(serial, one, browse_cancel=True)
        self.configure(serial, one)

        # Cancelling the configuration dialog must not ask about or drop unsaved changes.
        self.configure(serial, one, reject=True, modified=True)
        if not self.frame(serial).isWindowModified():
            raise AssertionError('Cancelling the dialog cleared the unsaved changes flag')
        # Cancelling the unsaved changes prompt keeps the current configuration.
        self.configure(
            serial, one, new_config=two, modified=True,
            unsaved_choice=QMessageBox.StandardButton.Cancel)
        if self.loaded_file(serial) != str(one):
            raise AssertionError('The config was replaced although the user cancelled')
        self.configure(serial, one)
        # Discarding the unsaved changes loads the new configuration.
        self.configure(
            serial, one, new_config=two, modified=True,
            unsaved_choice=QMessageBox.StandardButton.Discard)
        if self.loaded_file(serial) != str(two):
            raise AssertionError('The new config was not loaded after discarding changes')

        # An empty name is accepted without a warning and loads RViz's default config.
        self.configure(serial, two, new_config='')
        if self.loaded_file(serial) != 'default':
            raise AssertionError(
                f'An empty name left {self.loaded_file(serial)} loaded instead of the default')
        if self.frame(serial).isWindowModified():
            raise AssertionError('Loading the default config marked it as modified')
        self.configure(serial, '')

        # Configs opened or saved from RViz's own menu are shown in the dialog and saved
        # in the perspective.
        self.configure(serial, '', new_config=one)
        self.use_rviz_file_menu(serial, '&Open Config', two)
        self.unload(serial)
        self.load(serial, None)
        self.configure(serial, two)
        if self.loaded_file(serial) != str(two):
            raise AssertionError('The config opened from RViz was not restored')
        saved_as = one.with_name('saved_as.rviz')
        self.use_rviz_file_menu(serial, 'Save Config &As', saved_as)
        self.configure(serial, saved_as)
        self.unload(serial)

        # A startup config that cannot be resolved is reported in the RViz window.
        missing = 'package://missing_rqt_rviz_package/default.rviz'
        self.load(serial, missing)
        if 'Could not load display config' not in self.status_text(serial):
            raise AssertionError(
                f'The unresolved config was not reported: {self.status_text(serial)!r}')
        self.unload(serial)
        print('Review fix checks passed', flush=True)

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

        self.check_review_fixes()

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

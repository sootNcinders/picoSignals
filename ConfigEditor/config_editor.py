#!/usr/bin/env python3
"""
PicoSignals Configuration Editor
A multi-platform GUI application for creating, loading, and editing JSON configuration files.
Supports three device modes: Standard (full config), CTC (address only), and Overlay (partner-based).
"""

import sys
import json
import os
import copy
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QFormLayout, QGridLayout,
    QSpinBox, QDoubleSpinBox, QComboBox, QCheckBox, QLineEdit, QLabel,
    QPushButton, QGroupBox, QScrollArea, QTabWidget, QFileDialog, QMessageBox,
    QSplitter, QListWidget, QListWidgetItem, QInputDialog, QTableWidget,
    QTableWidgetItem, QHeaderView, QTextEdit
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QAction


class ConfigEditor(QMainWindow):
    """
    Main application window for editing PicoSignals configuration JSON files.
    
    Supports three device modes:
    - Standard: Full configuration with heads, pins, and all parameters
    - CTC: Simplified mode with address only (no heads/pins tabs)
    - Overlay: Head configuration loaded from partner device, battery values hardcoded to 1.0
    """
    def __init__(self):
        super().__init__()
        self.current_file = None
        self.config_data = self.get_default_config()
        self._last_head_index = None  # Track which head is currently displayed
        self._last_pin_index = None   # Track which pin is currently displayed
        self._current_tab = "General"  # Track which tab is currently active
        self._loading = False  # Flag to prevent saves during load
        self._head_data_to_load = None  # Store original head data during load
        self._visible_colors = []  # Track which colors should be visible (set by update_color_fields)
        self._pin_data_to_load = None  # Store original pin data during load (similar to _head_data_to_load)
        self.init_ui()
        self.load_config_to_ui()  # Ensure UI shows defaults at startup
        self.setWindowTitle("PicoSignals Configuration Editor")
        self.resize(1200, 800)

    def get_default_config(self):
        """Return default configuration dictionary for a new device.
        
        Contains default values for all parameters including mode, address, battery thresholds,
        and one head (head1) with color definitions.
        """
        return {
            "mode": "standard",
            "address": 1,
            "dimTime": 15,
            "sleepTime": 30,
            "lowBattery": 11.75,
            "batteryReset": 12.1,
            "batteryShutdown": 10.0,
            "retryTime": 100,
            "retries": 10,
            "ctcPresent": True,
            "monitorLEDs": 1,
            "head1": {
                "destination": [0, 0, 0, 0, 0, 0],
                "dim": 50,
                "green": {"pin": 0, "current": 30, "brightness": 255},
                "amber": {"pin": 0, "current": 30, "brightness": 255},
                "red": {"pin": 0, "current": 30, "brightness": 255}
            }
        }

    def init_ui(self):
        """Initialize the user interface with menu bar and tab widget.
        
        Creates four tabs:
        - General: Device mode, address, and mode-specific settings
        - Heads: Signal head configuration (Standard mode only)
        - Pins: Pin function definitions (Standard/Overlay modes)
        - JSON: Raw JSON editor for direct manipulation
        """
        # Create menu bar with File menu
        menubar = self.menuBar()
        file_menu = menubar.addMenu('File')

        new_action = QAction('New', self)
        new_action.triggered.connect(self.new_config)
        file_menu.addAction(new_action)

        open_action = QAction('Open', self)
        open_action.triggered.connect(self.open_config)
        file_menu.addAction(open_action)

        save_action = QAction('Save', self)
        save_action.triggered.connect(self.save_config)
        file_menu.addAction(save_action)

        save_as_action = QAction('Save As...', self)
        save_as_action.triggered.connect(self.save_config_as)
        file_menu.addAction(save_as_action)

        # Main widget and layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # Create tab widget - shows different configuration sections
        self.tab_widget = QTabWidget()
        self.tab_widget.currentChanged.connect(self.on_tab_changed)
        layout.addWidget(self.tab_widget)

        # Create each tab
        self.create_general_tab()
        self.create_heads_tab()
        self.create_pins_tab()
        self.create_json_tab()
        
        # Connect signals for real-time JSON preview
        self.connect_preview_signals()

    def connect_preview_signals(self):
        """Connect widget value changes to update JSON preview in real-time."""
        # General parameters - trigger save_ui_to_config when changed
        self.address_spin.valueChanged.connect(self.save_ui_to_config)
        self.dim_time_spin.valueChanged.connect(self.save_ui_to_config)
        self.sleep_time_spin.valueChanged.connect(self.save_ui_to_config)
        self.low_battery_spin.valueChanged.connect(self.save_ui_to_config)
        self.battery_reset_spin.valueChanged.connect(self.save_ui_to_config)
        self.battery_shutdown_spin.valueChanged.connect(self.save_ui_to_config)
        self.retry_time_spin.valueChanged.connect(self.save_ui_to_config)
        self.max_retries_spin.valueChanged.connect(self.save_ui_to_config)
        self.partner_spin.valueChanged.connect(self.save_ui_to_config)
        self.awake_pin_spin.valueChanged.connect(self.save_ui_to_config)
        self.mode_combo.currentIndexChanged.connect(self.save_ui_to_config)
        self.monitor_leds_combo.currentIndexChanged.connect(self.save_ui_to_config)
        self.ctc_present_check.stateChanged.connect(self.save_ui_to_config)

    def create_general_tab(self):
        """Create the General tab containing device settings.
        
        Tab includes:
        - Mode selector (Standard/CTC/Overlay) controls available tabs and features
        - Address (always visible) - unique device identifier 1-255
        - General parameters group (visibility depends on mode):
          * Standard: dim/sleep times, battery thresholds, retry time
          * CTC: hidden completely
          * Overlay: partner address only (dim/sleep/battery loaded from partner)
        """
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # Device mode selector - determines what features are available
        # Standard (full config), CTC (address only), Overlay (address + partner)
        mode_layout = QFormLayout()
        self.mode_combo = QComboBox()
        self.mode_combo.addItem("Standard", "standard")
        self.mode_combo.addItem("CTC", "ctc")
        self.mode_combo.addItem("Overlay", "overlay")
        self.mode_combo.currentIndexChanged.connect(self.on_mode_changed)
        mode_layout.addRow("Mode:", self.mode_combo)
        layout.addLayout(mode_layout)

        # Device address - always visible, required field
        address_layout = QFormLayout()
        self.address_spin = QSpinBox()
        self.address_spin.setRange(1, 255)
        address_layout.addRow("Address:", self.address_spin)
        layout.addLayout(address_layout)

        # General parameters group - contains mode-specific settings
        self.general_params_group = QGroupBox("General Parameters")
        form_layout = QFormLayout(self.general_params_group)

        # Dim Time: minutes before brightness is reduced
        self.dim_time_label = QLabel("Dim Time (minutes):")
        self.dim_time_spin = QSpinBox()
        self.dim_time_spin.setRange(0, 1000)
        self.dim_time_spin.setValue(15)  # Default value
        form_layout.addRow(self.dim_time_label, self.dim_time_spin)

        # Sleep Time: minutes before device powers down
        self.sleep_time_label = QLabel("Sleep Time (minutes):")
        self.sleep_time_spin = QSpinBox()
        self.sleep_time_spin.setRange(0, 1000)
        self.sleep_time_spin.setValue(30)  # Default value
        form_layout.addRow(self.sleep_time_label, self.sleep_time_spin)

        # Low Battery: voltage threshold for low battery warning
        self.low_battery_label = QLabel("Low Battery (V):")
        self.low_battery_spin = QDoubleSpinBox()
        self.low_battery_spin.setRange(0, 20)
        self.low_battery_spin.setSingleStep(0.1)
        self.low_battery_spin.setValue(11.75)  # Default value
        form_layout.addRow(self.low_battery_label, self.low_battery_spin)

        # Battery Reset: voltage at which low battery flag is cleared
        self.battery_reset_label = QLabel("Battery Reset (V):")
        self.battery_reset_spin = QDoubleSpinBox()
        self.battery_reset_spin.setRange(0, 20)
        self.battery_reset_spin.setSingleStep(0.1)
        self.battery_reset_spin.setValue(12.1)  # Default value
        form_layout.addRow(self.battery_reset_label, self.battery_reset_spin)

        # Battery Shutdown: voltage at which device shuts down to protect battery
        self.battery_shutdown_label = QLabel("Battery Shutdown (V):")
        self.battery_shutdown_spin = QDoubleSpinBox()
        self.battery_shutdown_spin.setRange(0, 20)
        self.battery_shutdown_spin.setSingleStep(0.1)
        self.battery_shutdown_spin.setValue(10.0)  # Default value
        form_layout.addRow(self.battery_shutdown_label, self.battery_shutdown_spin)

        # Retry Time: milliseconds to wait before retrying failed operations
        self.retry_time_spin = QSpinBox()
        self.retry_time_spin.setRange(0, 1000)
        self.retry_time_spin.setValue(100)  # Default value
        form_layout.addRow("Retry Time (ms):", self.retry_time_spin)

        # Max Retries: maximum number of retries for failed operations (available in all modes)
        self.max_retries_spin = QSpinBox()
        self.max_retries_spin.setRange(1, 255)
        self.max_retries_spin.setValue(10)  # Default value
        form_layout.addRow("Max Retries:", self.max_retries_spin)

        # Partner: device address to load head config from (Overlay mode only)
        # When in Overlay, this device doesn't define its own heads
        self.partner_label = QLabel("Partner:")
        self.partner_spin = QSpinBox()
        self.partner_spin.setRange(0, 255)
        self.partner_spin.setValue(99)  # Default value
        form_layout.addRow(self.partner_label, self.partner_spin)
        # Hide by default - only shown in Overlay mode
        self.partner_label.hide()
        self.partner_spin.hide()

        # CTC Present: whether CTC hardware is installed
        self.ctc_present_check = QCheckBox()
        self.ctc_present_check.setChecked(True)  # Default value
        form_layout.addRow("CTC Present:", self.ctc_present_check)

        # Monitor LEDs: level of LED circuit monitoring
        # 0 = off, 1 = open circuit detection, 2 = open + short circuit
        self.monitor_leds_combo = QComboBox()
        self.monitor_leds_combo.addItem("Off (no monitoring)", 0)
        self.monitor_leds_combo.addItem("Open circuit only", 1)
        self.monitor_leds_combo.addItem("Open + short circuit", 2)
        form_layout.addRow("Monitor LEDs:", self.monitor_leds_combo)

        # Awake Pin: GPIO pin to pull high when device wakes (Standard mode only)
        # 0-16 in UI, but 0 is saved as 255 to JSON
        self.awake_pin_label = QLabel("Awake Pin:")
        self.awake_pin_spin = QSpinBox()
        self.awake_pin_spin.setRange(0, 16)
        form_layout.addRow(self.awake_pin_label, self.awake_pin_spin)

        layout.addWidget(self.general_params_group)
        layout.addStretch()
        
        self.general_tab_index = self.tab_widget.addTab(tab, "General")

    def create_heads_tab(self):
        """Create the Heads tab for configuring signal heads.
        
        Allows configuration of 1-4 signal heads with:
        - Discrete mode: GPIO pins for Green/Amber/Red/Lunar LEDs
        - RGB mode: Single addressable RGB LED with pin, current, and RGB color values
        
        Tab is disabled in CTC and Overlay modes.
        """
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # Head selector to choose which head (1-4) to configure
        head_selector_layout = QHBoxLayout()
        head_selector_layout.addWidget(QLabel("Head:"))
        self.head_combo = QComboBox()
        self.head_combo.addItems(["1", "2", "3", "4"])
        self.head_combo.currentIndexChanged.connect(self.update_head_display)
        head_selector_layout.addWidget(self.head_combo)
        head_selector_layout.addStretch()
        layout.addLayout(head_selector_layout)

        # Label showing current selected head
        self.head_label = QLabel("head1")
        layout.addWidget(self.head_label)

        # Head settings group
        self.head_group = QGroupBox("Head Settings")
        head_layout = QFormLayout(self.head_group)

        # Head Style: Discrete (individual pins) vs RGB (addressable LED)
        self.head_style_combo = QComboBox()
        self.head_style_combo.addItems(["Discrete (GAR)", "RGB"])
        # Connect to both save and update to persist mode choice
        self.head_style_combo.currentTextChanged.connect(self.on_head_style_changed)
        head_layout.addRow("Head Style:", self.head_style_combo)

        # Head Mode: Standard or Dwarf
        self.head_mode_combo = QComboBox()
        self.head_mode_combo.addItem("Standard", "standard")
        self.head_mode_combo.addItem("Dwarf", "dwarf")
        self.head_mode_combo.currentTextChanged.connect(self.on_head_mode_changed)
        head_layout.addRow("Head Mode:", self.head_mode_combo)

        # Local Head Number: For Dwarf mode only
        self.local_head_num_label = QLabel("Local Head Number:")
        self.local_head_num_spin = QSpinBox()
        self.local_head_num_spin.setRange(0, 4)
        self.local_head_num_spin.setValue(0)
        head_layout.addRow(self.local_head_num_label, self.local_head_num_spin)
        # Initially hidden - will show when Dwarf mode is selected
        self.local_head_num_label.hide()
        self.local_head_num_spin.hide()

        # Destination: 6 values representing signal aspects
        dest_layout = QHBoxLayout()
        dest_layout.addWidget(QLabel("Destination:"))
        self.dest_spins = []
        for i in range(6):
            spin = QSpinBox()
            spin.setRange(0, 255)
            self.dest_spins.append(spin)
            dest_layout.addWidget(spin)
        head_layout.addRow(dest_layout)

        # Dim percentage (0-100%)
        # Stored as 0-255 in JSON, converted to/from percentage in UI
        self.head_dim_spin = QSpinBox()
        self.head_dim_spin.setRange(0, 100)
        head_layout.addRow("Dim (%):", self.head_dim_spin)

        # Release Time: minutes before aspect is released after triggered
        self.release_spin = QSpinBox()
        self.release_spin.setRange(1, 255)
        self.release_spin.setValue(6)  # Default value
        head_layout.addRow("Release Time (min):", self.release_spin)

        # Red Release Delay: seconds to delay red aspect release
        self.red_release_delay_spin = QSpinBox()
        self.red_release_delay_spin.setRange(0, 255)
        self.red_release_delay_spin.setValue(0)  # Default value
        head_layout.addRow("Red Release Delay (s):", self.red_release_delay_spin)

        # Color configuration groups - will be populated based on mode
        self.color_widgets = {}
        colors = ["green", "amber", "red", "blue", "lunar"]
        for color in colors:
            color_group = QGroupBox(color.capitalize())
            color_layout = QGridLayout(color_group)
            color_layout.setColumnStretch(0, 0)  # Labels narrow
            color_layout.setColumnStretch(1, 1)  # Fields wide
            self.color_widgets[color] = {'group': color_group, 'layout': color_layout}
            head_layout.addRow(color_group)

        # Initialize color fields for default mode (Discrete)
        self.update_color_fields()

        layout.addWidget(self.head_group)
        self.heads_tab_index = self.tab_widget.addTab(tab, "Heads")

    def on_head_style_changed(self):
        """Handle head style combo change (RGB vs Discrete) - save style and rebuild color fields."""
        # Save the style choice to the current head's config
        if hasattr(self, '_last_head_index'):
            head_index = self._last_head_index
            head_name = f"head{head_index}"
            if head_name not in self.config_data:
                self.config_data[head_name] = {}
            # Store style as explicit field: 'discrete' or 'rgb'
            style_text = self.head_style_combo.currentText()
            self.config_data[head_name]['style'] = 'rgb' if 'RGB' in style_text else 'discrete'
        # Now rebuild the color fields UI
        self.update_color_fields()

    def on_head_mode_changed(self):
        """Handle head mode combo change (Standard vs Dwarf) - show/hide local head number."""
        mode = self.head_mode_combo.currentData()
        
        if mode == "dwarf":
            self.local_head_num_label.show()
            self.local_head_num_spin.show()
        else:
            self.local_head_num_label.hide()
            self.local_head_num_spin.hide()
        
        # Save the mode choice to the current head's config
        if hasattr(self, '_last_head_index'):
            head_index = self._last_head_index
            head_name = f"head{head_index}"
            if head_name not in self.config_data:
                self.config_data[head_name] = {}
            self.config_data[head_name]['mode'] = mode

    def update_color_fields(self):
        """Rebuild color configuration widgets based on selected head style.
        
        Discrete mode: Shows pin, current, brightness for each color
        RGB mode: Shows pin/current for physical LEDs, RGB values for color aspects
        """
        style = self.head_style_combo.currentText()
        is_rgb = "RGB" in style

        # Determine which colors to display
        if is_rgb:
            colors = ["green", "amber", "red", "blue", "lunar"]
        else:
            colors = ["green", "amber", "red", "lunar"]
        
        # IMPORTANT: Store which colors are visible so update_color_values can use this
        # instead of calling isVisible(), which doesn't work immediately after show()
        self._visible_colors = colors

        # Update visibility and rebuild fields for each color
        all_colors = ["green", "amber", "red", "blue", "lunar"]
        for color in all_colors:
            if color in self.color_widgets:
                group = self.color_widgets[color]['group']
                if color in colors:
                    group.show()
                    layout = self.color_widgets[color]['layout']
                    # Disconnect signals from old widgets BEFORE clearing to prevent
                    # stale signals from firing after deleteLater() is called
                    self._disconnect_color_signals(color)
                    # Clear existing widgets
                    self.clear_layout(layout)
                    
                    # Build fresh widgets dictionary (clear old references to prevent accessing deleted widgets)
                    widgets_dict = {'group': self.color_widgets[color]['group'], 'layout': layout}

                    if is_rgb:
                        # RGB mode fields
                        row = 0
                        
                        if color in ["green", "red", "blue"]:
                            # Physical pins for these colors
                            pin_label = QLabel("Pin:")
                            pin_spin = QSpinBox()
                            pin_spin.setRange(0, 40)
                            layout.addWidget(pin_label, row, 0)
                            layout.addWidget(pin_spin, row, 1)
                            widgets_dict['pin'] = pin_spin
                            row += 1

                            current_label = QLabel("Current (mA):")
                            current_spin = QSpinBox()
                            current_spin.setRange(0, 100)
                            current_spin.setValue(30)  # Default: 30mA
                            layout.addWidget(current_label, row, 0)
                            layout.addWidget(current_spin, row, 1)
                            widgets_dict['current'] = current_spin
                            row += 1

                        if color in ["green", "amber", "red", "lunar"]:
                            # RGB values for color aspects
                            # Stored 0-255, displayed 0-100%
                            rgb_label = QLabel("RGB (%):")
                            rgb_layout = QHBoxLayout()
                            rgb_spins = []
                            for j in range(3):
                                rgb_spin = QSpinBox()
                                rgb_spin.setRange(0, 100)
                                rgb_spin.setValue(0)  # Default: no color
                                rgb_spins.append(rgb_spin)
                                rgb_layout.addWidget(rgb_spin)
                            layout.addWidget(rgb_label, row, 0)
                            layout.addLayout(rgb_layout, row, 1)
                            widgets_dict['rgb'] = rgb_spins
                            row += 1

                        self.color_widgets[color] = widgets_dict
                    else:
                        # Discrete mode fields
                        pin_label = QLabel("Pin:")
                        pin_spin = QSpinBox()
                        pin_spin.setRange(0, 40)
                        layout.addWidget(pin_label, 0, 0)
                        layout.addWidget(pin_spin, 0, 1)

                        current_label = QLabel("Current (mA):")
                        current_spin = QSpinBox()
                        current_spin.setRange(0, 100)
                        current_spin.setValue(30)  # Default: 30mA
                        layout.addWidget(current_label, 1, 0)
                        layout.addWidget(current_spin, 1, 1)

                        brightness_label = QLabel("Brightness (%):")
                        brightness_spin = QSpinBox()
                        brightness_spin.setRange(0, 100)
                        brightness_spin.setValue(100)  # Default: 100% brightness
                        layout.addWidget(brightness_label, 2, 0)
                        layout.addWidget(brightness_spin, 2, 1)

                        widgets_dict['pin'] = pin_spin
                        widgets_dict['current'] = current_spin
                        widgets_dict['brightness'] = brightness_spin
                        self.color_widgets[color] = widgets_dict
                else:
                    group.hide()

        # IMPORTANT: Connect color widget signals BEFORE setting values
        # Setting spinbox values will trigger valueChanged signals
        # We need the signal connections in place before they fire
        self.connect_head_widget_signals()

        # After rebuilding all color fields, reload values from current head
        # This ensures correct defaults appear when switching between modes
        # When we set values below, the signals will fire and call _save_current_head()
        
        # If we're in the middle of loading from update_head_display(), use the stored copy
        # which has the original data before any modifications
        if hasattr(self, '_head_data_to_load') and self._head_data_to_load is not None:
            self.update_color_values(self._head_data_to_load)
            self._head_data_to_load = None
        else:
            head_index = int(self.head_combo.currentText())
            head_name = f"head{head_index}"
            if head_name in self.config_data:
                # Reload existing head's color values into the newly created widgets
                self.update_color_values(self.config_data[head_name])
            else:
                # No head data yet - reset to defaults
                self.update_color_values({})

    def _disconnect_color_signals(self, color):
        """Disconnect all signals from a color's widgets before clearing layout."""
        if color not in self.color_widgets:
            return
        widgets = self.color_widgets[color]
        if 'pin' in widgets and widgets['pin']:
            try:
                widgets['pin'].valueChanged.disconnect()
            except TypeError:
                pass
        if 'current' in widgets and widgets['current']:
            try:
                widgets['current'].valueChanged.disconnect()
            except TypeError:
                pass
        if 'brightness' in widgets and widgets['brightness']:
            try:
                widgets['brightness'].valueChanged.disconnect()
            except TypeError:
                pass
        if 'rgb' in widgets and isinstance(widgets['rgb'], list):
            for rgb_spin in widgets['rgb']:
                try:
                    rgb_spin.valueChanged.disconnect()
                except TypeError:
                    pass

    def clear_layout(self, layout):
        """Recursively clear all widgets and sub-layouts from a layout."""
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
            else:
                sub_layout = item.layout()
                if sub_layout:
                    self.clear_layout(sub_layout)
                    sub_layout.deleteLater()

    def create_pins_tab(self):
        """Create the Pins tab for configuring pin functions.
        
        Pins define how external inputs/outputs are handled:
        - Standard mode: Capture (button press), Release (button release), Turnout (relay)
        - Overlay mode: ovlGreen/Amber/Red (outputs), ovlAuxIn (input)
        
        Tab is disabled in CTC mode.
        """
        tab = QWidget()
        layout = QVBoxLayout(tab)

        # Pin selector (pins 1-8)
        pin_selector_layout = QHBoxLayout()
        pin_selector_layout.addWidget(QLabel("Pin:"))
        self.pin_combo = QComboBox()
        self.pin_combo.addItems([str(i) for i in range(1, 9)])
        self.pin_combo.currentIndexChanged.connect(self.update_pin_display)
        pin_selector_layout.addWidget(self.pin_combo)
        pin_selector_layout.addStretch()
        layout.addLayout(pin_selector_layout)

        # Pin settings group
        self.pin_group = QGroupBox("Pin Settings")
        pin_layout = QFormLayout(self.pin_group)

        # Pin mode selector - available modes depend on device mode
        self.pin_mode_combo = QComboBox()
        self.pin_mode_combo.addItem("", "")
        self.pin_mode_combo.addItem("Capture", "capture")
        self.pin_mode_combo.addItem("Release", "release")
        self.pin_mode_combo.addItem("Turnout", "turnout")
        self.pin_mode_combo.addItem("ovlGreen", "ovlGreen")
        self.pin_mode_combo.addItem("ovlAmber", "ovlAmber")
        self.pin_mode_combo.addItem("ovlRed", "ovlRed")
        self.pin_mode_combo.addItem("ovlAuxIn", "ovlAuxIn")
        self.pin_mode_combo.currentIndexChanged.connect(self.update_pin_fields)
        pin_layout.addRow("Mode:", self.pin_mode_combo)

        # Dynamic parameters group
        self.pin_fields_group = QGroupBox("Parameters")
        self.pin_fields_layout = QFormLayout(self.pin_fields_group)
        pin_layout.addRow(self.pin_fields_group)

        # Dictionary to store dynamically created parameter widgets
        self.pin_params = {}

        layout.addWidget(self.pin_group)
        self.pins_tab_index = self.tab_widget.addTab(tab, "Pins")

    def create_json_tab(self):
        """Create the JSON tab for viewing and editing raw configuration."""
        tab = QWidget()
        layout = QVBoxLayout(tab)

        self.json_text = QTextEdit()
        self.json_text.setFontFamily("Courier New")
        layout.addWidget(self.json_text)

        update_button = QPushButton("Update from JSON")
        update_button.clicked.connect(self.update_from_json)
        layout.addWidget(update_button)

        self.tab_widget.addTab(tab, "JSON")

    def on_mode_changed(self):
        """Update UI based on selected device mode.
        
        When mode changes:
        1. Shows/hides parameter fields based on mode requirements
        2. Enables/disables tabs (CTC/Overlay disable Heads, CTC disables Pins)
        3. Rebuilds pin mode combo with available options
        4. Updates pin field widgets
        """
        mode = self.mode_combo.currentData()
        
        # Control parameter visibility - retryTime and max_retries are always visible
        if mode == "ctc":
            # In CTC mode, hide timing/battery parameters but keep retry settings
            self.dim_time_label.hide()
            self.dim_time_spin.hide()
            self.sleep_time_label.hide()
            self.sleep_time_spin.hide()
            self.low_battery_label.hide()
            self.low_battery_spin.hide()
            self.battery_reset_label.hide()
            self.battery_reset_spin.hide()
            self.battery_shutdown_label.hide()
            self.battery_shutdown_spin.hide()
            self.ctc_present_check.setVisible(True)
            self.partner_label.hide()
            self.partner_spin.hide()
            self.awake_pin_label.hide()
            self.awake_pin_spin.hide()
            self.monitor_leds_combo.setVisible(True)
            # Retry settings are visible for all modes
        else:
            self.general_params_group.show()
            
            if mode == "overlay":
                # Hide timing/battery (from partner)
                self.dim_time_label.hide()
                self.dim_time_spin.hide()
                self.sleep_time_label.hide()
                self.sleep_time_spin.hide()
                self.low_battery_label.hide()
                self.low_battery_spin.hide()
                self.battery_reset_label.hide()
                self.battery_reset_spin.hide()
                self.battery_shutdown_label.hide()
                self.battery_shutdown_spin.hide()
                # Show partner, hide awakePin
                self.partner_label.show()
                self.partner_spin.show()
                self.awake_pin_label.hide()
                self.awake_pin_spin.hide()
            else:  # Standard
                # Show all parameters
                self.dim_time_label.show()
                self.dim_time_spin.show()
                self.sleep_time_label.show()
                self.sleep_time_spin.show()
                self.low_battery_label.show()
                self.low_battery_spin.show()
                self.battery_reset_label.show()
                self.battery_reset_spin.show()
                self.battery_shutdown_label.show()
                self.battery_shutdown_spin.show()
                # Hide partner, show awakePin
                self.partner_label.hide()
                self.partner_spin.hide()
                self.awake_pin_label.show()
                self.awake_pin_spin.show()
        
        # Control tab availability
        if hasattr(self, 'heads_tab_index'):
            if mode == "ctc" or mode == "overlay":
                self.tab_widget.setTabEnabled(self.heads_tab_index, False)
            else:
                self.tab_widget.setTabEnabled(self.heads_tab_index, True)
        
        if hasattr(self, 'pins_tab_index'):
            if mode == "ctc":
                self.tab_widget.setTabEnabled(self.pins_tab_index, False)
            else:
                self.tab_widget.setTabEnabled(self.pins_tab_index, True)
        
        if not hasattr(self, 'pin_mode_combo'):
            return
            
        # Rebuild pin mode combo with mode-specific options
        self.pin_mode_combo.blockSignals(True)
        current_data = self.pin_mode_combo.currentData()
        self.pin_mode_combo.clear()
        
        if mode == "standard":
            self.pin_mode_combo.addItem("", "")
            self.pin_mode_combo.addItem("Capture", "capture")
            self.pin_mode_combo.addItem("Release", "release")
            self.pin_mode_combo.addItem("Turnout", "turnout")
        elif mode == "overlay":
            self.pin_mode_combo.addItem("", "")
            self.pin_mode_combo.addItem("ovlGreen", "ovlGreen")
            self.pin_mode_combo.addItem("ovlAmber", "ovlAmber")
            self.pin_mode_combo.addItem("ovlRed", "ovlRed")
            self.pin_mode_combo.addItem("ovlAuxIn", "ovlAuxIn")
        else:  # CTC
            self.pin_mode_combo.addItem("", "")
        
        # Restore previous selection if possible
        index = self.pin_mode_combo.findData(current_data)
        if index >= 0:
            self.pin_mode_combo.setCurrentIndex(index)
        else:
            self.pin_mode_combo.setCurrentIndex(0)
        
        self.pin_mode_combo.blockSignals(False)
        
        # IMPORTANT: Don't rebuild pin fields during load_config_to_ui
        # update_pin_display() already creates/restores pin fields with correct values.
        # Rebuilding here would overwrite those values with defaults.
        if not self._loading:
            self.update_pin_fields()
    
    def update_pin_fields(self):
        """Rebuild pin parameter widgets based on selected mode.
        
        Different pin modes require different parameters:
        - Release: single head
        - Capture: head1 (required), head2, turnout (optional)
        - Turnout: no parameters
        - Overlay modes: single head
        - ovlAuxIn: no parameters
        """
        mode = self.pin_mode_combo.currentData()
        
        # Clear existing parameter widgets
        while self.pin_fields_layout.rowCount():
            self.pin_fields_layout.removeRow(0)
        
        self.pin_params = {}
        
        if mode == "release":
            head_spin = QSpinBox()
            head_spin.setRange(0, 4)
            self.pin_fields_layout.addRow("Head:", head_spin)
            self.pin_params['head'] = head_spin
            
        elif mode == "capture":
            head1_spin = QSpinBox()
            head1_spin.setRange(0, 4)
            self.pin_fields_layout.addRow("Head1:", head1_spin)
            self.pin_params['head1'] = head1_spin
            
            head2_spin = QSpinBox()
            head2_spin.setRange(0, 4)
            self.pin_fields_layout.addRow("Head2 (optional):", head2_spin)
            self.pin_params['head2'] = head2_spin
            
            turnout_spin = QSpinBox()
            turnout_spin.setRange(0, 8)
            self.pin_fields_layout.addRow("Turnout Pin (optional):", turnout_spin)
            self.pin_params['turnout'] = turnout_spin
            
        elif mode == "turnout":
            label = QLabel("No additional parameters needed")
            self.pin_fields_layout.addRow(label)
            
        elif mode in ["ovlGreen", "ovlAmber", "ovlRed"]:
            head_spin = QSpinBox()
            head_spin.setRange(0, 4)
            self.pin_fields_layout.addRow("Head:", head_spin)
            self.pin_params['head'] = head_spin
            
        elif mode == "ovlAuxIn":
            label = QLabel("No additional parameters needed")
            self.pin_fields_layout.addRow(label)
    
    def update_pin_display(self):
        """Load pin configuration when pin selection changes.
        
        Process:
        1. Save previous pin's data (if switching pins)
        2. Get selected pin number
        3. Look up pin data in config_data
        4. Block signals to prevent double-calling update_pin_fields
        5. Set pin mode and rebuild widgets
        6. Restore parameter values from config
        """
        # Prevent any saves while we're loading and rebuilding the display
        was_loading = self._loading
        self._loading = True
        
        try:
            pin_num = int(self.pin_combo.currentText())
            pin_key = f'pin{pin_num}'
            
            # Save previous pin if we're switching to a different pin (but not during initial load)
            if not was_loading and self._last_pin_index and self._last_pin_index != pin_num:
                self._loading = was_loading  # Restore flag to allow save
                self.save_pin_to_config(self._last_pin_index)
                self._loading = True  # Re-set to prevent saves while loading new pin
            
            self._last_pin_index = pin_num  # Track current pin
            
            if pin_key in self.config_data:
                # Make a deep copy of pin_data to preserve original during load
                pin_data = copy.deepcopy(self.config_data[pin_key])
                mode = pin_data.get('mode', '')
                
                # IMPORTANT: Store the pin data to preserve it during field rebuild
                # (similar to _head_data_to_load for colors)
                self._pin_data_to_load = pin_data
                
                index = self.pin_mode_combo.findData(mode)
                
                # Always set the combo, even if index is invalid (will set to blank)
                # Block signals to prevent double-call to update_pin_fields
                self.pin_mode_combo.blockSignals(True)
                if index >= 0:
                    self.pin_mode_combo.setCurrentIndex(index)
                else:
                    # Mode not found, set to blank/default
                    self.pin_mode_combo.setCurrentIndex(0)
                self.pin_mode_combo.blockSignals(False)
                
                # Always rebuild fields based on current combo value
                self.update_pin_fields()
                
                # Restore parameter values if they were created
                if hasattr(self, 'pin_params') and self.pin_params:
                    # Get current mode from combo (in case it was set above)
                    current_mode = self.pin_mode_combo.currentData()
                    # Use _pin_data_to_load if available (has original data before any modifications)
                    restore_data = self._pin_data_to_load if hasattr(self, '_pin_data_to_load') and self._pin_data_to_load else {}
                    try:
                        if current_mode == 'capture':
                            if 'head1' in self.pin_params and 'head1' in restore_data:
                                self.pin_params['head1'].blockSignals(True)
                                self.pin_params['head1'].setValue(restore_data['head1'])
                                self.pin_params['head1'].blockSignals(False)
                            if 'head2' in self.pin_params and 'head2' in restore_data:
                                self.pin_params['head2'].blockSignals(True)
                                self.pin_params['head2'].setValue(restore_data['head2'])
                                self.pin_params['head2'].blockSignals(False)
                            if 'turnout' in self.pin_params and 'turnout' in restore_data:
                                self.pin_params['turnout'].blockSignals(True)
                                self.pin_params['turnout'].setValue(restore_data['turnout'])
                                self.pin_params['turnout'].blockSignals(False)
                        elif current_mode in ['release', 'ovlGreen', 'ovlAmber', 'ovlRed']:
                            if 'head' in self.pin_params and 'head' in restore_data:
                                self.pin_params['head'].blockSignals(True)
                                self.pin_params['head'].setValue(restore_data['head'])
                                self.pin_params['head'].blockSignals(False)
                    except (RuntimeError, KeyError) as e:
                        # Widget might have been deleted or key doesn't exist
                        print(f"Error setting pin parameter: {e}")
                    # NOTE: Do NOT clear _pin_data_to_load here!
                    # Keep it so save_pin_to_config() can use it as fallback if spinboxes were reset
            else:
                # No data for this pin
                self._pin_data_to_load = None
                index = self.pin_mode_combo.findData("")
                if index >= 0:
                    self.pin_mode_combo.blockSignals(True)
                    self.pin_mode_combo.setCurrentIndex(index)
                    self.pin_mode_combo.blockSignals(False)
                self.update_pin_fields()
            
            # Connect pin widget signals for real-time preview
            self.connect_pin_widget_signals()
        finally:
            # Restore loading flag
            self._loading = was_loading

    def connect_pin_widget_signals(self):
        """Connect pin widget signals to save_pin_to_config for real-time preview."""
        if not self._loading and hasattr(self, 'pin_mode_combo'):
            # Connect pin mode combo - use wrapper for correct pin
            try:
                self.pin_mode_combo.currentIndexChanged.disconnect()
                self.pin_mode_combo.currentIndexChanged.connect(self._save_current_pin)
            except:
                pass
            
            # Connect pin parameters if they exist
            if hasattr(self, 'pin_params') and self.pin_params:
                for key, widget in self.pin_params.items():
                    if hasattr(widget, 'valueChanged'):
                        try:
                            widget.valueChanged.disconnect()
                            widget.valueChanged.connect(self._save_current_pin)
                        except:
                            pass

    def update_head_display(self):
        """Load and display configuration for selected head.
        
        Process:
        1. Save previous head's data (if switching heads)
        2. Get selected head number
        3. Load destination and dim from config
        4. Detect mode based on what's in config (has RGB or brightness?)
        5. Rebuild color fields for detected mode
        6. Populate values from config
        """
        # Prevent any saves while we're loading and rebuilding the display
        was_loading = self._loading
        self._loading = True
        
        try:
            head_index = int(self.head_combo.currentText())
            
            # Save previous head if we're switching to a different head (but not during initial load)
            if not was_loading and self._last_head_index and self._last_head_index != head_index:
                self._loading = was_loading  # Restore flag to allow save
                self.save_head_to_config(self._last_head_index)
                self._loading = True  # Re-set to prevent saves while loading new head
            
            head_name = f"head{head_index}"
            self.head_label.setText(head_name)
            self._last_head_index = head_index  # Track current head
            
            if head_name in self.config_data:
                # IMPORTANT: Make a deep copy of head_data to preserve original during load
                head_data = copy.deepcopy(self.config_data[head_name])
                
                # Block signals while loading to prevent cascading saves
                self.dest_spins[0].blockSignals(True) if self.dest_spins else None
                
                # Load destination values
                for i, dest in enumerate(head_data.get('destination', [0]*6)):
                    self.dest_spins[i].setValue(dest)

                # Load dim (convert 0-255 to 0-100%)
                raw_dim = head_data.get('dim', 50)
                self.head_dim_spin.blockSignals(True)
                self.head_dim_spin.setValue(min(100, max(0, round(raw_dim * 100 / 255))))
                self.head_dim_spin.blockSignals(False)

                # Load release time (default 6 minutes)
                self.release_spin.blockSignals(True)
                self.release_spin.setValue(head_data.get('release', 6))
                self.release_spin.blockSignals(False)

                # Load red release delay (default 0 seconds)
                self.red_release_delay_spin.blockSignals(True)
                self.red_release_delay_spin.setValue(head_data.get('redReleaseDelay', 0))
                self.red_release_delay_spin.blockSignals(False)
                
                # Unblock destination signals
                for spin in self.dest_spins:
                    spin.blockSignals(False)

                # Detect style (RGB vs Discrete): Infer from actual data, use data field as hint only
                # RGB mode is only true if ANY color has RGB data or blue is present
                has_rgb_data = any('rgb' in head_data.get(color, {}) for color in ["green", "amber", "red", "blue", "lunar"])
                has_blue = 'blue' in head_data
                is_rgb_mode = has_rgb_data or has_blue
                
                # Set style combo to match detected style
                # Block signals while setting programmatically to prevent on_head_style_changed() from firing
                self.head_style_combo.blockSignals(True)
                if is_rgb_mode:
                    self.head_style_combo.setCurrentText("RGB")
                else:
                    self.head_style_combo.setCurrentText("Discrete (GAR)")
                self.head_style_combo.blockSignals(False)

                # Load head mode (Standard vs Dwarf)
                head_mode = head_data.get('mode', 'standard')
                self.head_mode_combo.blockSignals(True)
                index = self.head_mode_combo.findData(head_mode)
                if index >= 0:
                    self.head_mode_combo.setCurrentIndex(index)
                else:
                    self.head_mode_combo.setCurrentIndex(0)  # Default to Standard
                self.head_mode_combo.blockSignals(False)

                # Load local head number if dwarf mode
                if head_mode == 'dwarf':
                    self.local_head_num_label.show()
                    self.local_head_num_spin.show()
                    self.local_head_num_spin.blockSignals(True)
                    self.local_head_num_spin.setValue(head_data.get('localHeadNum', 0))
                    self.local_head_num_spin.blockSignals(False)
                else:
                    self.local_head_num_label.hide()
                    self.local_head_num_spin.hide()

                # Rebuild and populate colors
                # Store the original head_data so update_color_fields() can use it
                self._head_data_to_load = head_data
                self.update_color_fields()
            else:
                # Reset to defaults
                for i in range(6):
                    self.dest_spins[i].setValue(0)
                self.head_dim_spin.setValue(50)
                self.release_spin.setValue(6)
                self.red_release_delay_spin.setValue(0)
                # Create new head with standard mode and discrete style as default
                self.config_data[head_name] = {'mode': 'standard'}
                
                # Set style to discrete (default)
                self.head_style_combo.blockSignals(True)
                self.head_style_combo.setCurrentText("Discrete (GAR)")
                self.head_style_combo.blockSignals(False)
                
                # Set mode to standard (default)
                self.head_mode_combo.blockSignals(True)
                self.head_mode_combo.setCurrentIndex(0)
                self.head_mode_combo.blockSignals(False)
                
                # Hide local head number by default
                self.local_head_num_label.hide()
                self.local_head_num_spin.hide()
                self.local_head_num_spin.setValue(0)
                
                # No data to load
                self._head_data_to_load = {}
                self.update_color_fields()
                self.update_color_values({})
            
            # Connect head widget signals for real-time preview
            self.connect_head_widget_signals()
        finally:
            # Restore loading flag
            self._loading = was_loading
    
    def connect_head_widget_signals(self):
        """Connect head widget signals to save_head_to_config for real-time preview."""
        # Connect all head widget signals regardless of loading state
        # We need to connect newly created color widgets even during initial load
        for spin in self.dest_spins:
            try:
                spin.valueChanged.disconnect()  # Disconnect any previous connections
            except TypeError:
                pass
            # Use wrapper to ensure correct head is saved
            spin.valueChanged.connect(self._save_current_head)
        
        try:
            self.head_dim_spin.valueChanged.disconnect()
        except TypeError:
            pass
        self.head_dim_spin.valueChanged.connect(self._save_current_head)
        
        try:
            self.release_spin.valueChanged.disconnect()
        except TypeError:
            pass
        self.release_spin.valueChanged.connect(self._save_current_head)
        
        try:
            self.red_release_delay_spin.valueChanged.disconnect()
        except TypeError:
            pass
        self.red_release_delay_spin.valueChanged.connect(self._save_current_head)
        
        try:
            self.head_mode_combo.currentIndexChanged.disconnect()
        except TypeError:
            pass
        self.head_mode_combo.currentIndexChanged.connect(self._save_current_head)
        
        try:
            self.local_head_num_spin.valueChanged.disconnect()
        except TypeError:
            pass
        self.local_head_num_spin.valueChanged.connect(self._save_current_head)
        
        # Connect color widget signals
        for color, widgets in self.color_widgets.items():
            if 'pin' in widgets and widgets['pin']:
                try:
                    widgets['pin'].valueChanged.disconnect()
                except TypeError:
                    pass
                widgets['pin'].valueChanged.connect(self._save_current_head)
            if 'current' in widgets and widgets['current']:
                try:
                    widgets['current'].valueChanged.disconnect()
                    widgets['current'].valueChanged.connect(self._save_current_head)
                except TypeError:
                    widgets['current'].valueChanged.connect(self._save_current_head)
            if 'brightness' in widgets and widgets['brightness']:
                try:
                    widgets['brightness'].valueChanged.disconnect()
                    widgets['brightness'].valueChanged.connect(self._save_current_head)
                except TypeError:
                    widgets['brightness'].valueChanged.connect(self._save_current_head)
            if 'rgb' in widgets and isinstance(widgets['rgb'], list):
                for rgb_spin in widgets['rgb']:
                    try:
                        rgb_spin.valueChanged.disconnect()
                        rgb_spin.valueChanged.connect(self._save_current_head)
                    except TypeError:
                        rgb_spin.valueChanged.connect(self._save_current_head)


    def _save_current_head(self):
        """Wrapper to save the currently displayed head. Skips saving during initial load."""
        # Don't save during initial load
        if getattr(self, '_loading', False):
            return
        if hasattr(self, '_last_head_index') and self._last_head_index:
            self.save_head_to_config(self._last_head_index)

    def _save_current_pin(self):
        """Wrapper to save the currently displayed pin."""
        if hasattr(self, '_last_pin_index') and self._last_pin_index:
            self.save_pin_to_config(self._last_pin_index)

    def on_tab_changed(self, index):
        """Handle tab changes.
        
        Data is saved automatically via signal connections when user edits.
        We only need to:
        1. Save when switching heads/pins within the Heads/Pins tabs
        2. Update the JSON preview when entering JSON tab
        """
        # Don't process during initial load
        if self._loading:
            return
            
        current_tab_text = self.tab_widget.tabText(index)
        previous_tab = getattr(self, '_current_tab', None)
        self._current_tab = current_tab_text
        
        # Refresh head/pin display when ENTERING their tabs
        # (This loads the saved data, doesn't overwrite it)
        if current_tab_text == "Heads":
            self.update_head_display()
        elif current_tab_text == "Pins":
            self.update_pin_display()
        
        # Update JSON preview when entering JSON tab
        # DO NOT save here or it will delete the color data!
        elif current_tab_text == "JSON":
            self.update_json_preview()

    def update_json_preview(self):
        """Update the JSON preview in the JSON tab to reflect current config_data."""
        json_str = json.dumps(self.config_data, indent=2)
        self.json_text.setText(json_str)

    def update_color_values(self, head_data):
        """Populate color widget values from config data.
        
        Converts stored percentages (0-255) to display format (0-100%)
        for brightness and RGB values.
        """
        # Use _visible_colors if set (after update_color_fields runs)
        # Otherwise fall back to checking isVisible()
        visible_colors = getattr(self, '_visible_colors', None)
        
        for color, widgets in self.color_widgets.items():
            # Determine if this color should be visible
            if visible_colors is not None:
                is_visible = color in visible_colors
            else:
                is_visible = widgets['group'].isVisible() if 'group' in widgets else False
            
            # Safety check: ensure widgets dict has required keys
            if 'group' not in widgets:
                continue
                
            if color in head_data and is_visible:
                color_data = head_data[color]
                # Only set values if the widget exists (handles mode differences)
                if 'pin' in widgets and widgets['pin'] is not None:
                    try:
                        widgets['pin'].blockSignals(True)
                        pin_val = color_data.get('pin', 0)
                        widgets['pin'].setValue(pin_val)
                        widgets['pin'].blockSignals(False)
                    except RuntimeError:
                        # Widget was deleted, skip it
                        pass
                if 'current' in widgets and widgets['current'] is not None:
                    try:
                        widgets['current'].blockSignals(True)
                        widgets['current'].setValue(color_data.get('current', 30))
                        widgets['current'].blockSignals(False)
                    except RuntimeError:
                        # Widget was deleted, skip it
                        pass
                if 'rgb' in widgets and isinstance(widgets['rgb'], list):
                    rgb = color_data.get('rgb', [0, 0, 0])
                    for j, val in enumerate(rgb):
                        try:
                            widgets['rgb'][j].blockSignals(True)
                            widgets['rgb'][j].setValue(round(val * 100 / 255))
                            widgets['rgb'][j].blockSignals(False)
                        except RuntimeError:
                            # Widget was deleted, skip it
                            pass
                if 'brightness' in widgets and widgets['brightness'] is not None:
                    try:
                        widgets['brightness'].blockSignals(True)
                        raw_brightness = color_data.get('brightness', 255)
                        widgets['brightness'].setValue(round(raw_brightness * 100 / 255))
                        widgets['brightness'].blockSignals(False)
                    except RuntimeError:
                        # Widget was deleted, skip it
                        pass
            elif 'group' in widgets and is_visible:
                # Reset to defaults
                if 'pin' in widgets and widgets['pin'] is not None:
                    try:
                        widgets['pin'].blockSignals(True)
                        widgets['pin'].setValue(0)
                        widgets['pin'].blockSignals(False)
                    except RuntimeError:
                        pass
                if 'current' in widgets and widgets['current'] is not None:
                    try:
                        widgets['current'].blockSignals(True)
                        widgets['current'].setValue(30)
                        widgets['current'].blockSignals(False)
                    except RuntimeError:
                        pass
                if 'rgb' in widgets and isinstance(widgets['rgb'], list):
                    for rgb_spin in widgets['rgb']:
                        try:
                            rgb_spin.blockSignals(True)
                            rgb_spin.setValue(0)
                            rgb_spin.blockSignals(False)
                        except RuntimeError:
                            pass
                if 'brightness' in widgets and widgets['brightness'] is not None:
                    try:
                        widgets['brightness'].blockSignals(True)
                        widgets['brightness'].setValue(100)
                        widgets['brightness'].blockSignals(False)
                    except RuntimeError:
                        pass

    def load_config_to_ui(self):
        """Load config_data into all UI widgets.
        
        Important: Load mode FIRST to populate pin_mode_combo before loading pins.
        
        Data conversions:
        - Dim/Brightness: stored 0-255, displayed 0-100%
        - RGB: stored 0-255, displayed 0-100%
        """
        # Set flag to prevent saves during load
        self._loading = True
        
        # Reset tracking indices to ensure fresh load
        self._last_head_index = None
        self._last_pin_index = None
        
        # Load mode first - triggers on_mode_changed
        mode = self.config_data.get('mode', 'standard')
        index = self.mode_combo.findData(mode)
        self.mode_combo.setCurrentIndex(index if index >= 0 else 0)
        
        # Load general parameters
        self.address_spin.setValue(self.config_data.get('address', 1))
        self.partner_spin.setValue(self.config_data.get('partner', 99))
        self.dim_time_spin.setValue(self.config_data.get('dimTime', 15))
        self.sleep_time_spin.setValue(self.config_data.get('sleepTime', 30))
        self.low_battery_spin.setValue(self.config_data.get('lowBattery', 11.75))
        self.battery_reset_spin.setValue(self.config_data.get('batteryReset', 12.1))
        self.battery_shutdown_spin.setValue(self.config_data.get('batteryShutdown', 10.0))
        self.retry_time_spin.setValue(self.config_data.get('retryTime', 100))
        self.max_retries_spin.setValue(self.config_data.get('retries', 10))
        self.ctc_present_check.setChecked(self.config_data.get('ctcPresent', True))
        monitor_mode = self.config_data.get('monitorLEDs', self.config_data.get('monitorLEDS', 1))
        index = self.monitor_leds_combo.findData(monitor_mode)
        self.monitor_leds_combo.setCurrentIndex(index if index >= 0 else 0)
        
        # Load Awake Pin - convert 255 from JSON to 0 in UI
        awake_pin_value = self.config_data.get('awakePin', 0)
        if awake_pin_value == 255:
            awake_pin_value = 0
        self.awake_pin_spin.setValue(awake_pin_value)

        # Load heads
        available_heads = [h for h in ["head1", "head2", "head3", "head4"] if h in self.config_data]
        if available_heads:
            first_head = available_heads[0]
            first_index = int(first_head[-1])
            self.head_combo.blockSignals(True)
            self.head_combo.setCurrentIndex(first_index - 1)
            self.head_combo.blockSignals(False)
        # Always call directly to ensure fresh load
        self.update_head_display()

        # Load pins
        self.pin_combo.blockSignals(True)
        self.pin_combo.setCurrentIndex(0)
        self.pin_combo.blockSignals(False)
        # Always call directly to ensure fresh load
        self.update_pin_display()

        # Explicitly call on_mode_changed to ensure visibility is set correctly
        # This is necessary because setting the mode combo to its current value
        # may not trigger the signal, so visibility won't be updated
        self.on_mode_changed()

        # Update JSON preview
        self.json_text.setText(json.dumps(self.config_data, indent=2))
        
        # Clear loading flag - now edits will be saved
        self._loading = False
        
        # Update JSON preview with loaded config
        self.update_json_preview()

    def save_head_to_config(self, head_index=None):
        """Save currently displayed head from UI to config_data.
        
        Captures all head parameters (destination, dim, mode, localHeadNum if dwarf, colors) from the UI widgets
        and saves them back to config_data. Called before switching heads or tabs
        to preserve edits.
        
        Args:
            head_index: Optional head number to save. If None, uses current combo selection.
                       Needed when switching heads since combo has already changed.
        """
        # Use provided head_index, or fall back to current combo selection
        if head_index is None:
            head_index = int(self.head_combo.currentText())
        head_name = f"head{head_index}"
        
        # Start fresh - build head data completely from widgets to avoid any shared references
        # between heads
        head_data = {}
        
        # Save head mode (Standard or Dwarf)
        head_data['mode'] = self.head_mode_combo.currentData()
        
        # Save local head number if in dwarf mode
        if head_data['mode'] == 'dwarf':
            head_data['localHeadNum'] = self.local_head_num_spin.value()
        
        # Update destination and dim from UI (always create fresh lists/values)
        head_data['destination'] = [spin.value() for spin in self.dest_spins]
        # Convert 0-100% back to 0-255 for storage
        head_data['dim'] = int(round(self.head_dim_spin.value() * 255 / 100))
        
        # Save release time and red release delay
        head_data['release'] = self.release_spin.value()
        head_data['redReleaseDelay'] = self.red_release_delay_spin.value()
        
        # Save color-specific data for visible colors
        for color, widgets in self.color_widgets.items():
            if 'group' not in widgets:
                continue
            is_visible = widgets['group'].isVisible()
            if not is_visible:
                continue
            
            color_data = {}
            
            # Try to read pin value
            pin_value = 0
            if 'pin' in widgets and widgets['pin'] is not None:
                try:
                    pin_value = widgets['pin'].value()
                except Exception as e:
                    pin_value = 0
            
            # If color has a pin assigned, save it and all related data
            if pin_value > 0:
                color_data['pin'] = pin_value
                
                # Save current (always try to save, don't skip defaults)
                if 'current' in widgets and widgets['current'] is not None:
                    try:
                        current_val = widgets['current'].value()
                        color_data['current'] = current_val
                    except:
                        pass
                
                # Save brightness for discrete mode (convert 0-100% to 0-255)
                if 'brightness' in widgets and widgets['brightness'] is not None:
                    try:
                        brightness_val = int(round(widgets['brightness'].value() * 255 / 100))
                        color_data['brightness'] = brightness_val
                    except:
                        pass
                
                # Save RGB if present (convert 0-100% to 0-255)
                if 'rgb' in widgets and isinstance(widgets['rgb'], list):
                    try:
                        rgb = [int(round(spin.value() * 255 / 100)) for spin in widgets['rgb']]
                        if any(rgb):
                            color_data['rgb'] = rgb
                    except:
                        pass
            
            # Save color if it has a pin
            if color_data:
                head_data[color] = color_data
            else:
                # Remove color if it has no pin
                head_data.pop(color, None)
        
        # Check if head has meaningful data to save
        # Save if it has color data OR if it has non-default destination/dim/release/redReleaseDelay
        has_color_data = any(key in head_data for key in ['green', 'amber', 'red', 'blue', 'lunar'])
        has_config_data = (
            head_data.get('destination') != [0, 0, 0, 0, 0, 0] or
            head_data.get('dim', 128) != 128 or  # 128 is 50% of 255
            head_data.get('release', 6) != 6 or
            head_data.get('redReleaseDelay', 0) != 0 or
            head_data.get('mode') != 'standard' or
            head_data.get('localHeadNum') is not None
        )
        
        if has_color_data or has_config_data:
            self.config_data[head_name] = head_data
        else:
            # Only remove if it's truly empty (all defaults)
            self.config_data.pop(head_name, None)
        
        # Update JSON preview to reflect new head state
        self.update_json_preview()

    def save_pin_to_config(self, pin_num=None):
        """Save currently displayed pin from UI to config_data.
        
        Similar to save_head_to_config, this captures pin parameters before switching.
        Includes guards to prevent errors if widgets don't exist.
        
        Args:
            pin_num: Optional pin number to save. If None, uses current combo selection.
        """
        # Guard against missing widget state
        if not hasattr(self, 'pin_params') or not self.pin_params:
            return
        
        if pin_num is None:
            pin_num = int(self.pin_combo.currentText())
        
        pin_mode = self.pin_mode_combo.currentData()
        
        if pin_mode:
            pin_data = {'mode': pin_mode}
            
            # IMPORTANT: If we have cached pin data (_pin_data_to_load), use that as the base
            # This prevents losing data if spinboxes have been recreated with default values
            if hasattr(self, '_pin_data_to_load') and self._pin_data_to_load:
                # Copy all non-mode fields from cached data
                for key, val in self._pin_data_to_load.items():
                    if key != 'mode':
                        pin_data[key] = val
            
            # Try to read from spinboxes, but don't override cached data if spinbox is 0
            if pin_mode == 'release':
                if self.pin_params.get('head') and hasattr(self.pin_params['head'], 'value'):
                    val = self.pin_params['head'].value()
                    if val > 0:
                        pin_data['head'] = val
            elif pin_mode == 'capture':
                if self.pin_params.get('head1') and hasattr(self.pin_params['head1'], 'value'):
                    val = self.pin_params['head1'].value()
                    if val > 0:
                        pin_data['head1'] = val
                if self.pin_params.get('head2') and hasattr(self.pin_params['head2'], 'value'):
                    val = self.pin_params['head2'].value()
                    if val > 0:
                        pin_data['head2'] = val
                if self.pin_params.get('turnout') and hasattr(self.pin_params['turnout'], 'value'):
                    val = self.pin_params['turnout'].value()
                    if val > 0:
                        pin_data['turnout'] = val
            elif pin_mode == 'turnout':
                pass
            elif pin_mode in ['ovlGreen', 'ovlAmber', 'ovlRed']:
                if self.pin_params.get('head') and hasattr(self.pin_params['head'], 'value'):
                    val = self.pin_params['head'].value()
                    if val > 0:
                        pin_data['head'] = val
            
            # Only save pin if it has actual configuration data (not just a mode)
            # Check if there's anything other than 'mode' in the pin_data
            has_config_data = any(key != 'mode' for key in pin_data.keys())
            if has_config_data:
                self.config_data[f'pin{pin_num}'] = pin_data
            else:
                # Don't save empty pins - remove from config_data if it exists
                self.config_data.pop(f'pin{pin_num}', None)
            
            # Clear cached data AFTER saving
            if pin_num == int(self.pin_combo.currentText()):
                self._pin_data_to_load = None
        else:
            self.config_data.pop(f'pin{pin_num}', None)
            self._pin_data_to_load = None
        
        # Update JSON preview to reflect new pin state
        self.update_json_preview()

    def save_ui_to_config(self):
        """Save all UI widget values to config_data.
        
        Key behaviors:
        - Saves currently displayed head and pin
        - Cleans up mode-specific parameters not applicable to current mode
        - Cleans up empty heads from config_data
        - Removes heads/pins for non-standard modes
        - Overlay mode hardcodes battery to 1.0
        - Converts percentages (0-100% UI) to storage (0-255)
        """
        # First, save the currently displayed head and pin to config_data
        if self._last_head_index:
            self.save_head_to_config(self._last_head_index)
        # Save current pin if we have a pin tracked
        if self._last_pin_index:
            self.save_pin_to_config(self._last_pin_index)
        
        mode = self.mode_combo.currentData()
        
        # Always save these parameters for all modes
        self.config_data['mode'] = mode
        self.config_data['address'] = self.address_spin.value()
        self.config_data['retryTime'] = self.retry_time_spin.value()
        self.config_data['retries'] = self.max_retries_spin.value()
        self.config_data['monitorLEDs'] = self.monitor_leds_combo.currentData()
        
        # Mode-specific parameter handling
        if mode == "standard":
            # Standard mode: save all timing and battery parameters
            self.config_data['dimTime'] = self.dim_time_spin.value()
            self.config_data['sleepTime'] = self.sleep_time_spin.value()
            self.config_data['lowBattery'] = self.low_battery_spin.value()
            self.config_data['batteryReset'] = self.battery_reset_spin.value()
            self.config_data['batteryShutdown'] = self.battery_shutdown_spin.value()
            self.config_data['ctcPresent'] = self.ctc_present_check.isChecked()
            
            # Save Awake Pin - convert 0 in UI to 255 in JSON
            awake_pin_value = self.awake_pin_spin.value()
            if awake_pin_value == 0:
                self.config_data['awakePin'] = 255
            else:
                self.config_data['awakePin'] = awake_pin_value
            
            # Remove overlay-only parameter
            self.config_data.pop('partner', None)
            
            # Keep heads and pins (they may exist from previous loads)
            
        elif mode == "overlay":
            # Overlay mode: save partner, remove timing/battery/awakePin
            self.config_data['partner'] = self.partner_spin.value()
            self.config_data['lowBattery'] = 1.0
            self.config_data['batteryReset'] = 1.0
            self.config_data['batteryShutdown'] = 1.0
            self.config_data['monitorLEDs'] = self.monitor_leds_combo.currentData()
            
            # Remove mode-specific parameters not used in overlay
            self.config_data.pop('dimTime', None)
            self.config_data.pop('sleepTime', None)
            self.config_data.pop('awakePin', None)
            self.config_data.pop('ctcPresent', None)
            
            # Remove heads and pins (overlay doesn't define them)
            for head_name in ['head1', 'head2', 'head3', 'head4']:
                self.config_data.pop(head_name, None)
            for pin_num in range(1, 33):
                self.config_data.pop(f'pin{pin_num}', None)
                
        elif mode == "ctc":
            # CTC mode: minimal parameters
            self.config_data['ctcPresent'] = self.ctc_present_check.isChecked()
            
            # Remove all timing/battery/mode-specific parameters
            self.config_data.pop('dimTime', None)
            self.config_data.pop('sleepTime', None)
            self.config_data.pop('lowBattery', None)
            self.config_data.pop('batteryReset', None)
            self.config_data.pop('batteryShutdown', None)
            self.config_data.pop('awakePin', None)
            self.config_data.pop('partner', None)
            
            # Remove heads and pins (CTC doesn't define them)
            for head_name in ['head1', 'head2', 'head3', 'head4']:
                self.config_data.pop(head_name, None)
            for pin_num in range(1, 33):
                self.config_data.pop(f'pin{pin_num}', None)
        
        # Clean up empty heads - only keep heads that have actual data
        heads_to_remove = []
        for head_name in ['head1', 'head2', 'head3', 'head4']:
            if head_name in self.config_data:
                head_data = self.config_data[head_name]
                # Remove the mode field if it exists (internal state)
                head_data.pop('mode', None)
                # Check if head is empty (only has destination and dim which are defaults)
                has_color_data = any(key in head_data for key in ['green', 'amber', 'red', 'blue', 'lunar'])
                if not has_color_data:
                    # If it only has default destination and dim, don't save it
                    if head_data.get('destination') == [0, 0, 0, 0, 0, 0] and head_data.get('dim', 0) <= 50:
                        heads_to_remove.append(head_name)
        
        # Remove empty heads
        for head_name in heads_to_remove:
            self.config_data.pop(head_name, None)
        
        # Update JSON preview to reflect new config state
        self.update_json_preview()

    def new_config(self):
        """Create new configuration with defaults."""
        self.config_data = self.get_default_config()
        self.load_config_to_ui()
        self.current_file = None
        self.setWindowTitle("PicoSignals Configuration Editor - New")

    def open_config(self):
        """Open and load JSON configuration file."""
        file_name, _ = QFileDialog.getOpenFileName(self, "Open Configuration", "", "JSON files (*.json)")
        if file_name:
            try:
                with open(file_name, 'r') as f:
                    self.config_data = json.load(f)
                self.load_config_to_ui()
                self.current_file = file_name
                self.setWindowTitle(f"PicoSignals Configuration Editor - {os.path.basename(file_name)}")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to load file: {str(e)}")

    def save_config(self):
        """Save configuration to file."""
        if self.current_file:
            self.save_ui_to_config()
            try:
                with open(self.current_file, 'w') as f:
                    json.dump(self.config_data, f, indent=2)
                QMessageBox.information(self, "Success", "Configuration saved successfully!")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save file: {str(e)}")
        else:
            self.save_config_as()

    def save_config_as(self):
        """Save configuration with new filename."""
        file_name, _ = QFileDialog.getSaveFileName(self, "Save Configuration", "", "JSON files (*.json)")
        if file_name:
            self.current_file = file_name
            self.save_config()

    def update_from_json(self):
        """Load configuration from JSON text editor."""
        try:
            self.config_data = json.loads(self.json_text.toPlainText())
            self.load_config_to_ui()
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Invalid JSON: {str(e)}")


def main():
    app = QApplication(sys.argv)
    editor = ConfigEditor()
    editor.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

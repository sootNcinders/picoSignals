import sys
import re
import time
import threading
import serial
import serial.tools.list_ports
import binascii
from pathlib import Path
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QComboBox, QTableWidget, QTableWidgetItem,
    QFileDialog, QProgressBar, QSpinBox, QDialog
)
from PyQt5.QtCore import Qt, pyqtSignal, QObject, QTimer
from PyQt5.QtGui import QFont


class UpdateWorker(QObject):
    """Worker thread for OTA update operations"""
    progress_update = pyqtSignal(int)  # percentage
    status_update = pyqtSignal(str)
    time_update = pyqtSignal(str)
    error_signal = pyqtSignal(str)
    finished_signal = pyqtSignal()
    version_update = pyqtSignal(int, str)  # node_id, version

    def __init__(self, hex_file, nodes, serial_port):
        super().__init__()
        self.hex_file = hex_file
        self.nodes = nodes
        self.serial_port = serial_port
        self.is_running = True
        self.start_time = None
        self.term = None

    def update_elapsed_time(self):
        """Update elapsed time display"""
        if self.start_time:
            elapsed = time.perf_counter() - self.start_time
            hours = int(elapsed // 3600)
            minutes = int((elapsed % 3600) // 60)
            seconds = int(elapsed % 60)
            self.time_update.emit(f"{hours:02d}:{minutes:02d}:{seconds:02d}")

    def run(self):
        """Main update process"""
        try:
            self.start_time = time.perf_counter()
            self.status_update.emit("Opening serial connection...")

            # Open serial connection
            self.term = serial.Serial(self.serial_port, 115200, timeout=0.01)
            self.status_update.emit(f"Connected to {self.serial_port}")

            # Read hex file
            with open(self.hex_file, "r") as f:
                lines = [l.rstrip("\r\n") for l in f.readlines()]

            # Prepare nodes for update
            any_node_ready = False
            ready_nodes = []

            # Check which nodes are ready
            self.status_update.emit("Checking nodes for readiness...")
            for node in self.nodes:
                if not self.is_running:
                    break

                node_ready = False
                i = 0
                while not node_ready and i < 10:
                    # Send Update Command
                    ln_out = "~"
                    ln_out += f'{node:0>2X}'
                    ln_out += "UPDATE\n"
                    self.term.write(ln_out.encode("ASCII"))

                    # Wait for device to reboot
                    time.sleep(2)

                    ln_out = "*E"
                    ln_out += f'{node:0>2X}'
                    ln_out += "\n"
                    self.term.write(ln_out.encode("ASCII"))

                    t = time.perf_counter()
                    while not node_ready and time.perf_counter() - t < 10:
                        ln_in = self.term.readline()
                        if len(ln_in) > 0:
                            ln_in = ln_in.decode("ASCII", errors='ignore')
                            try:
                                addr = (int(ln_in[2], 16) << 4) + int(ln_in[3], 16)
                            except (ValueError, IndexError):
                                addr = 0

                            if (ln_in[0] == '*' and ln_in[1] == 'A' and
                                    addr == node):
                                any_node_ready = True
                                node_ready = True
                                ready_nodes.append(node)
                                self.status_update.emit(
                                    f"Node {node} is ready for update")

                    i += 1
                self.update_elapsed_time()

            if not any_node_ready:
                self.status_update.emit("Nodes did not respond, skipping update")
                self.finished_signal.emit()
                return

            self.status_update.emit(
                f"Updating {len(ready_nodes)} nodes with file {self.hex_file}")

            # Send file to nodes
            file_checksum = 0
            num_lines = len(lines)
            i = 0

            while i < num_lines:
                if not self.is_running:
                    break

                rec_num = i + 1
                line = lines[i]

                # Convert hex line to bytes
                if len(line) < 2:
                    payload = b''
                else:
                    try:
                        payload = binascii.unhexlify(line[1:])
                    except (binascii.Error, TypeError):
                        payload = b''

                # Build output line
                ln_out = "*D"
                ln_out += line
                ln_out += f'{((rec_num >> 8) & 0xFF):0>2X}'
                ln_out += f'{((rec_num) & 0xFF):0>2X}'
                ln_out += "\n"

                self.term.write(ln_out.encode("ASCII"))

                # Update checksum
                b_out = bytearray()
                b_out.extend(b'*D')
                b_out.append(ord(line[0]) if line else 0)
                b_out.extend(payload)
                file_checksum += sum(b_out[2:])

                # Update progress
                perc = int((rec_num / max(1, num_lines)) * 100)
                self.progress_update.emit(perc)
                self.status_update.emit(f"Sending record {rec_num}/{num_lines}")

                # Delay for flash writes
                t = time.perf_counter()
                delay = 0.15
                if len(line) > 7 and line[5] == 'F' and line[6] == '0':
                    delay = 0.25

                while time.perf_counter() - t < delay:
                    if not self.is_running:
                        break

                    ln_in = self.term.readline()
                    if len(ln_in) > 0:
                        ln_in = ln_in.decode("ASCII", errors='ignore')
                        if (len(ln_in) > 0 and ln_in[0] == '*' and
                                len(ln_in) >= 4 and ln_in[1] == 'N'):
                            try:
                                last_rec_num = (int(ln_in[4], 16) << 12) + (
                                    int(ln_in[5], 16) << 8) + (
                                    int(ln_in[6], 16) << 4) + int(ln_in[7], 16)
                            except (ValueError, IndexError):
                                last_rec_num = 0

                            if last_rec_num != 0 and last_rec_num < rec_num:
                                self.status_update.emit(
                                    f"Resending from record {last_rec_num}")
                                rec_num = last_rec_num - 1
                                i = rec_num - 1

                self.update_elapsed_time()
                i += 1

            if not self.is_running:
                self.status_update.emit("Update cancelled")
                self.finished_signal.emit()
                return

            # Send checksum
            chk = file_checksum & 0xFFFF
            self.status_update.emit("Sending checksum...")

            for _ in range(10):
                ln_out = "*C"
                ln_out += f'{((chk >> 8) & 0xFF):0>2X}'
                ln_out += f'{((chk) & 0xFF):0>2X}'
                ln_out += "\n"
                self.term.write(ln_out.encode("ASCII"))
                time.sleep(1)

            # Verify versions
            self.status_update.emit("Verifying node versions...")
            versions = []

            for node in ready_nodes:
                if not self.is_running:
                    break

                ver_received = False
                i = 0
                while not ver_received and i < 10:
                    ln_out = "~"
                    ln_out += f'{node:0>2X}'
                    ln_out += "ERR CLR\n"
                    self.term.write(ln_out.encode("ASCII"))

                    ln_out = "~"
                    ln_out += f'{node:0>2X}'
                    ln_out += "VER\n"
                    self.term.write(ln_out.encode("ASCII"))

                    time.sleep(5)

                    ln_in = self.term.readline()
                    while len(ln_in) > 0 and not ver_received:
                        ln_in = ln_in.decode("ASCII", errors='ignore')

                        try:
                            addr = (int(ln_in[1], 16) << 4) + int(ln_in[2], 16)
                        except (ValueError, IndexError):
                            addr = 0

                        if (ln_in[3] == '>' and ln_in[4] == ' ' and
                                ln_in[5] == 'V' and addr == node):
                            ver_received = True
                            ver = ln_in[4:].strip()
                            versions.append((node, ver))
                            self.version_update.emit(node, ver)

                        ln_in = self.term.readline()

                    i += 1

                if not ver_received:
                    versions.append((node, "V0R0"))
                    self.version_update.emit(node, "V0R0 (unverified)")

            self.progress_update.emit(100)
            self.status_update.emit("Update complete!")
            self.update_elapsed_time()

        except Exception as e:
            self.error_signal.emit(f"Error: {str(e)}")
        finally:
            if self.term:
                self.term.close()
            self.finished_signal.emit()

    def stop(self):
        """Stop the update process"""
        self.is_running = False


class OTAUpdaterGUI(QMainWindow):
    """Main GUI window for OTA Updater"""

    def __init__(self):
        super().__init__()
        self.worker = None
        self.worker_thread = None
        self.timer = None
        self.init_ui()
        self.update_serial_ports()

    def init_ui(self):
        """Initialize the user interface"""
        self.setWindowTitle("Pico Signals OTA Updater")
        self.setGeometry(100, 100, 700, 600)

        # Main widget and layout
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout()

        # File selection
        file_layout = QHBoxLayout()
        file_label = QLabel("Hex File:")
        self.file_path_label = QLabel("No file selected")
        self.file_path_label.setStyleSheet("color: gray;")
        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self.browse_file)
        file_layout.addWidget(file_label)
        file_layout.addWidget(self.file_path_label)
        file_layout.addWidget(browse_btn)
        layout.addLayout(file_layout)

        # Version display
        version_layout = QHBoxLayout()
        version_label = QLabel("Version:")
        self.version_display = QLabel("Not selected")
        self.version_display.setStyleSheet("color: white;")
        version_layout.addWidget(version_label)
        version_layout.addWidget(self.version_display)
        version_layout.addStretch()
        layout.addLayout(version_layout)

        # Serial port selection
        port_layout = QHBoxLayout()
        port_label = QLabel("Serial Port:")
        self.port_combo = QComboBox()
        refresh_ports_btn = QPushButton("Refresh Ports")
        refresh_ports_btn.clicked.connect(self.update_serial_ports)
        port_layout.addWidget(port_label)
        port_layout.addWidget(self.port_combo)
        port_layout.addWidget(refresh_ports_btn)
        layout.addLayout(port_layout)

        # Node management
        nodes_label = QLabel("Update Nodes:")
        layout.addWidget(nodes_label)

        nodes_layout = QHBoxLayout()
        self.nodes_list = QTableWidget()
        self.nodes_list.setColumnCount(2)
        self.nodes_list.setHorizontalHeaderLabels(["Node Address", "Version"])
        self.nodes_list.setMaximumHeight(150)
        self.nodes_list.setColumnWidth(0, 150)
        self.nodes_list.setColumnWidth(1, 150)
        nodes_layout.addWidget(self.nodes_list)

        nodes_btn_layout = QVBoxLayout()
        add_node_btn = QPushButton("Add Node")
        add_node_btn.clicked.connect(self.add_node)
        remove_node_btn = QPushButton("Remove Selected")
        remove_node_btn.clicked.connect(self.remove_node)
        nodes_btn_layout.addWidget(add_node_btn)
        nodes_btn_layout.addWidget(remove_node_btn)
        nodes_btn_layout.addStretch()
        nodes_layout.addLayout(nodes_btn_layout)
        layout.addLayout(nodes_layout)

        # Progress section
        progress_label = QLabel("Update Progress:")
        layout.addWidget(progress_label)

        self.progress_bar = QProgressBar()
        self.progress_bar.setMinimum(0)
        self.progress_bar.setMaximum(100)
        layout.addWidget(self.progress_bar)

        # Status display
        info_layout = QHBoxLayout()
        self.percentage_label = QLabel("0%")
        self.percentage_label.setFont(QFont("Arial", 12, QFont.Bold))
        self.time_label = QLabel("00:00:00")
        self.time_label.setFont(QFont("Arial", 12, QFont.Bold))
        info_layout.addWidget(self.percentage_label)
        info_layout.addStretch()
        info_layout.addWidget(self.time_label)
        layout.addLayout(info_layout)

        # Status message
        self.status_label = QLabel("Ready")
        self.status_label.setStyleSheet("color: #87CEEB;")
        self.status_label.setWordWrap(True)
        layout.addWidget(self.status_label)

        # Control buttons
        button_layout = QHBoxLayout()
        self.start_btn = QPushButton("Start Update")
        self.start_btn.clicked.connect(self.start_update)
        self.cancel_btn = QPushButton("Cancel")
        self.cancel_btn.clicked.connect(self.cancel_update)
        self.cancel_btn.setEnabled(False)
        button_layout.addWidget(self.start_btn)
        button_layout.addWidget(self.cancel_btn)
        layout.addLayout(button_layout)

        main_widget.setLayout(layout)

        # Initialize node 0
        self.add_node_with_id(0)

    def browse_file(self):
        """Open file browser to select hex file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select Hex File", "", "Hex Files (*.hex);;All Files (*)"
        )
        if file_path:
            self.hex_file = file_path
            filename = Path(file_path).name
            self.file_path_label.setText(filename)

            # Extract version from filename
            version = self.extract_version(filename)
            if version:
                self.version_display.setText(version)
                self.version_display.setStyleSheet("color: white;")
            else:
                self.version_display.setText("Could not extract version")
                self.version_display.setStyleSheet("color: red;")

    def extract_version(self, filename):
        """Extract version from filename like picoSignals-V3R4.hex"""
        match = re.search(r'V\d+R\d+', filename)
        if match:
            return match.group(0)
        return None

    def update_serial_ports(self):
        """Update the list of available serial ports"""
        self.port_combo.clear()
        ports = [port.device for port in serial.tools.list_ports.comports()]
        if ports:
            self.port_combo.addItems(ports)
        else:
            self.port_combo.addItem("No ports available")

    def add_node(self):
        """Open dialog to add a new node"""
        dialog = QDialog(self)
        dialog.setWindowTitle("Add Node")
        dialog.setGeometry(self.x() + 100, self.y() + 100, 300, 100)

        layout = QVBoxLayout()
        label = QLabel("Enter node address (0-254):")
        spin_box = QSpinBox()
        spin_box.setMinimum(0)
        spin_box.setMaximum(254)
        spin_box.setValue(1)

        ok_btn = QPushButton("OK")
        cancel_btn = QPushButton("Cancel")

        def add_node_action():
            node_id = spin_box.value()
            self.add_node_with_id(node_id)
            dialog.accept()

        ok_btn.clicked.connect(add_node_action)
        cancel_btn.clicked.connect(dialog.reject)

        layout.addWidget(label)
        layout.addWidget(spin_box)

        btn_layout = QHBoxLayout()
        btn_layout.addWidget(ok_btn)
        btn_layout.addWidget(cancel_btn)
        layout.addLayout(btn_layout)

        dialog.setLayout(layout)
        dialog.exec_()

    def add_node_with_id(self, node_id):
        """Add a node with given ID if not already present"""
        # Check if node already exists
        for i in range(self.nodes_list.rowCount()):
            item = self.nodes_list.item(i, 0)
            if item and item.data(Qt.UserRole) == node_id:
                return

        row = self.nodes_list.rowCount()
        self.nodes_list.insertRow(row)
        
        node_item = QTableWidgetItem(f"Node {node_id}")
        node_item.setData(Qt.UserRole, node_id)
        node_item.setFlags(node_item.flags() & ~Qt.ItemIsEditable)
        self.nodes_list.setItem(row, 0, node_item)
        
        version_item = QTableWidgetItem("Pending")
        version_item.setFlags(version_item.flags() & ~Qt.ItemIsEditable)
        self.nodes_list.setItem(row, 1, version_item)

    def remove_node(self):
        """Remove selected node from list"""
        current_row = self.nodes_list.currentRow()
        if current_row >= 0:
            # Don't allow removing node 0
            node_item = self.nodes_list.item(current_row, 0)
            if node_item:
                node_id = node_item.data(Qt.UserRole)
                if node_id == 0:
                    self.status_label.setText("Cannot remove Node 00 (required)")
                    self.status_label.setStyleSheet("color: red;")
                    return
                self.nodes_list.removeRow(current_row)

    def get_nodes(self):
        """Get list of node IDs from the table widget"""
        nodes = []
        for i in range(self.nodes_list.rowCount()):
            item = self.nodes_list.item(i, 0)
            if item:
                nodes.append(item.data(Qt.UserRole))
        return nodes

    def start_update(self):
        """Start the OTA update process"""
        # Validation
        if not hasattr(self, 'hex_file'):
            self.status_label.setText("Please select a hex file")
            self.status_label.setStyleSheet("color: red;")
            return

        if not self.hex_file or not Path(self.hex_file).exists():
            self.status_label.setText("Hex file does not exist")
            self.status_label.setStyleSheet("color: red;")
            return

        if self.port_combo.currentText() == "No ports available":
            self.status_label.setText("No serial port available")
            self.status_label.setStyleSheet("color: red;")
            return

        nodes = self.get_nodes()
        if not nodes:
            self.status_label.setText("Please add at least one node")
            self.status_label.setStyleSheet("color: red;")
            return

        # Reset UI
        self.progress_bar.setValue(0)
        self.percentage_label.setText("0%")
        self.time_label.setText("00:00:00")
        self.start_btn.setEnabled(False)
        self.cancel_btn.setEnabled(True)

        # Reset version column in table
        for i in range(self.nodes_list.rowCount()):
            version_item = self.nodes_list.item(i, 1)
            if version_item:
                version_item.setText("Pending")

        # Create worker and thread
        self.worker = UpdateWorker(
            self.hex_file, nodes, self.port_combo.currentText()
        )
        self.worker_thread = threading.Thread(target=self.worker.run)

        # Connect signals
        self.worker.progress_update.connect(self.update_progress)
        self.worker.status_update.connect(self.update_status)
        self.worker.time_update.connect(self.update_time)
        self.worker.error_signal.connect(self.show_error)
        self.worker.version_update.connect(self.update_node_version)
        self.worker.finished_signal.connect(self.update_finished)

        # Start timer for time updates
        self.timer = QTimer()
        self.timer.timeout.connect(self.worker.update_elapsed_time)
        self.timer.start(1000)  # Update every second

        # Start worker thread
        self.worker_thread.start()

    def update_progress(self, value):
        """Update progress bar"""
        self.progress_bar.setValue(value)
        self.percentage_label.setText(f"{value}%")

    def update_status(self, message):
        """Update status message"""
        self.status_label.setText(message)
        self.status_label.setStyleSheet("color: #87CEEB;")

    def update_time(self, time_str):
        """Update elapsed time display"""
        self.time_label.setText(time_str)

    def show_error(self, error_msg):
        """Display error message"""
        self.status_label.setText(error_msg)
        self.status_label.setStyleSheet("color: red;")

    def update_node_version(self, node_id, version):
        """Update version display for a node in the table"""
        for i in range(self.nodes_list.rowCount()):
            item = self.nodes_list.item(i, 0)
            if item and item.data(Qt.UserRole) == node_id:
                version_item = self.nodes_list.item(i, 1)
                if version_item:
                    version_item.setText(version)
                break

    def cancel_update(self):
        """Cancel the update process"""
        if self.worker:
            self.worker.stop()
            self.status_label.setText("Cancelling update...")

    def update_finished(self):
        """Called when update process finishes"""
        self.start_btn.setEnabled(True)
        self.cancel_btn.setEnabled(False)

        if self.timer:
            self.timer.stop()

        if self.worker_thread:
            self.worker_thread.join(timeout=5)


def main():
    app = QApplication(sys.argv)
    window = OTAUpdaterGUI()
    window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()

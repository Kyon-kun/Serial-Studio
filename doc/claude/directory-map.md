# Directory Map

```
app/src/
├── IO/              ConnectionManager, DeviceManager, CircularBuffer, FrameReader, FrameConfig
│   ├── Drivers/     UART, Network, BluetoothLE, Audio, CANBus, HID, Modbus, MQTT, Process, USB
│   └── FileTransmission/  Protocol base, XMODEM, YMODEM, ZMODEM, CRC utilities
├── DataModel/       Frame, FrameBuilder, FrameConsumer, DataTable(Store), ExportSchema,
│   │                ProjectModel, ProjectEditor, NotificationCenter
│   ├── Project/     ProjectModel TU split (Crud, Folders, Loading, Persistence, Sources,
│   │                Tables, Workspaces + ProjectModelShared.h) and ProjectEditor TU split
│   │                (Commit, Forms, Mqtt, MultiSelect, Selection, Summaries, Tree, Wiring +
│   │                ProjectEditorItemIds.h, ProjectEditorShared.h); facades stay in DataModel/
│   ├── Scripting/   IScriptEngine, FrameParser, JsScriptEngine, JsWatchdog,
│   │                LuaScriptEngine, LuaCompat, ScriptTemplates
│   ├── Editors/     JsCodeEditor, OutputCodeEditor, PainterCodeEditor,
│   │                DatasetTransformEditor, CodeFormatter
│   ├── Importers/   DBCImporter, ModbusMapImporter, ProtoImporter
│   └── Dialogs/     TransmitTestDialog
├── UI/              Dashboard, Taskbar (workspaces), visualization + output widget types
│   └── Widgets/Output/  Button, Toggle, Slider, TextField, Panel (+ PanelLayout), Base
├── API/             TCP server port 7777 (MCP + legacy JSON-RPC), 30 handlers
│   └── Handlers/    per-command handlers; ProjectHandler split into ProjectHandler{File,
│                    Entities,Parser,Batch}.cpp + ProjectApiSupport.h (registration stays
│                    in ProjectHandler.cpp)
├── Console/         Terminal + export
├── CSV/ MDF4/       File playback & export
├── Sessions/  (Pro) DatabaseManager + Sessions::Export + Sessions::Player
├── MQTT/            Publisher (FrameConsumer-based, threaded, rate-limited 1-30 Hz)
├── Licensing/       LemonSqueezy, Trial, MachineID, CommercialToken (FeatureTier)
├── Platform/        CSD, NativeWindow (true-size CSD windows: no painted shadow; Win10 gets a
│                    DWM-drawn shadow via WM_NCCALCSIZE filter, Linux the 1px border)
├── Misc/            JsonValidator, ThemeManager, ModuleManager
├── AppState.h       Singleton: OperationMode, projectFilePath, FrameConfig
├── SerialStudio.h   Central enums (BusType, OperationMode, FrameDetection)
└── Concepts.h       C++20 concepts
app/qml/             DatabaseExplorer/, MainWindow/, ProjectEditor/, Widgets/, Dialogs/
lib/                 KissFFT, QCodeEditor, mdflib, OpenSSL, lua54, QuaZip, hidapi, QSimpleUpdater
```

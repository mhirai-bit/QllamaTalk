import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.VectorImage

Item {
    id: root

    required property string title
    required property list<Component> model

    height: mainRow.height + (internals.expanded ? expandingMenu.contentHeight : 0)
    width: mainRow.width + expandingMenu.tabWidth

    QtObject {
        id: internals
        property bool expanded: false
    }

    RowLayout {
        id: mainRow

        Label {
            id: titleLabel
            text: root.title
            font.bold: true
            font.pointSize: 24
            Layout.alignment: Qt.AlignVCenter
        }

        VectorImage {
            id: iconImage
            rotation: internals.expanded ? 90 : 0
            Behavior on rotation {
                RotationAnimation {
                    duration: 200
                    easing.type: Easing.InCubic
                }
            }
            source: "icons/arrow.svg"
            MouseArea {
                anchors.fill: parent
                onClicked: internals.expanded = !internals.expanded
            }
            Layout.alignment: Qt.AlignVCenter
        }
    }

    ListView {
        id: expandingMenu
        readonly property int tabWidth: mainRow.width/5
        width: root.width
        height: internals.expanded ? contentHeight : 0
        spacing: 8
        opacity: internals.expanded ? 1.0 : 0.0

        Behavior on height {
            NumberAnimation {
                duration: 400
                easing.type: Easing.InCubic
            }
        }
        Behavior on opacity {
            NumberAnimation {
                duration: 400
                easing.type: Easing.InCubic
            }
        }

        anchors {
            top: mainRow.bottom
            topMargin: 16
            left: mainRow.left
            leftMargin: expandingMenu.tabWidth
        }

        model: root.model
        delegate: Loader {
            sourceComponent: modelData
        }
    }
}

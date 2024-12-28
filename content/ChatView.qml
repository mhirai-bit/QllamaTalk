// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Alexey Edelev <semlanik@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import content

Rectangle {
    id: root
    anchors.fill: parent
    color: "#09102b"

    // Optional: set focus to input on visible
    onVisibleChanged: {
        if (root.visible) {
            _inputField.forceActiveFocus()
        }
    }

    ListView {
        id: messageListView
        anchors.top: parent.top
        anchors.bottom: _inputField.top
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        model: LlamaChatEngine.messages

        delegate: Item {
            // top-level item for each message
            width: root.width
            height: _outerWrapper.height + 10

            Item {
                id: _outerWrapper
                width: parent.width / 2 - 20

                // The Column below will determine total height
                height: _messageColumn.height + 20

                // Decide if it's a user message or assistant
                property bool ownMessage: (model.sender === "user")

                anchors {
                    right: _outerWrapper.ownMessage ? parent.right : undefined
                    left:  _outerWrapper.ownMessage ? undefined   : parent.left
                    rightMargin: _outerWrapper.ownMessage ? 10 : 0
                    leftMargin:  _outerWrapper.ownMessage ? 0  : 10
                    verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    color: _outerWrapper.ownMessage ? "#9d9faa" : "#53586b"
                    border.color:  "#41cd52"
                    border.width: 1
                }

                // The main content container
                Column {
                    id: _messageColumn
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: 10
                        rightMargin: 10
                        verticalCenter: parent.verticalCenter
                    }

                    // Dynamically compute total height from children
                    height: _userName.implicitHeight + textBody.implicitHeight

                    // Sender label (You / AI)
                    Text {
                        id: _userName
                        property string from: _outerWrapper.ownMessage ? qsTr("You") : qsTr("AI")
                        anchors.left: parent.left
                        anchors.right: parent.right
                        font.pointSize: 12
                        font.weight: Font.Bold
                        color: "#f3f3f4"
                        text: from + ": "
                    }

                    // The actual message text
                    Text {
                        id: textBody
                        anchors.left: parent.left
                        anchors.right: parent.right
                        font.pointSize: 12
                        color: "#f3f3f4"
                        wrapMode: Text.Wrap
                        text: model.messageContent
                    }
                }
            }
        }

        // scroll to end when count changes
        onCountChanged: {
            Qt.callLater(messageListView.positionViewAtEnd)
        }
    }

    // Chat input area
    ChatInputField {
        id: _inputField
        focus: true
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 20
        }

        placeholderText: qsTr("Start typing here or paste an image")
        onAccepted: {
            LlamaChatEngine.setUser_input(_inputField.text)
            _inputField.text = ""
        }
    }
}

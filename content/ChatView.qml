// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Alexey Edelev <semlanik@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick
import content

Rectangle {
    id: root
    anchors.fill: parent
    color: "#09102b"
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
        model: LlamaChatEngine.messages
        clip: true
        delegate: Item {
            height: _imageWrapper.height + 10
            width: root.width
            Item {
                id: _imageWrapper
                height: _messageColumn.height + 20
                width: parent.width/2 - 20
                property bool ownMessage: model.sender === "user"
                anchors{
                    right: _imageWrapper.ownMessage ? parent.right : undefined
                    left: _imageWrapper.ownMessage ? undefined : parent.left
                    rightMargin: _imageWrapper.ownMessage ? 10 : 0
                    leftMargin: _imageWrapper.ownMessage ? 0 : 10
                    verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    color: _imageWrapper.ownMessage ? "#9d9faa" : "#53586b"
                    border.color:  "#41cd52"
                    border.width: 1
                }

                Column {
                    id: _messageColumn
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: 10
                        rightMargin: 10
                        verticalCenter: parent.verticalCenter
                    }
                    // Text {
                    //     id: _userName
                    //     property string from: _imageWrapper.ownMessage ? qsTr("You") : model.from
                    //     anchors.left: parent.left
                    //     anchors.right: parent.right
                    //     font.pointSize: 12
                    //     font.weight: Font.Bold
                    //     color: "#f3f3f4"
                    //     text: _userName.from + ": "
                    // }

                    Loader {
                        id: delegateLoader
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: item ? item.height : 0
                        sourceComponent: textDelegate
                        onItemChanged:  {
                            if (item) {
                                item.content = model.messageContent
                            }
                        }
                    }
                }
            }
        }
        onCountChanged: {
            Qt.callLater( messageListView.positionViewAtEnd )
        }
    }

    Component {
        id: textDelegate
        Item {
            property alias content: innerText.text
            height: childrenRect.height
            Text {
                id: innerText
                anchors.left: parent.left
                anchors.right: parent.right
                height: implicitHeight
                font.pointSize: 12
                color: "#f3f3f4"
                wrapMode: Text.Wrap
            }
        }
    }

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

        Keys.onPressed: (event)=>  {
            if (event.key === Qt.Key_V && event.modifiers & Qt.ControlModifier) {
                console.log("Ctrl + V")
                switch (LlamaChatEngine.clipBoardContentType) {
                case LlamaChatEngine.Text:
                    _inputField.paste()
                    break
                case LlamaChatEngine.Image:
                    LlamaChatEngine.sendImageFromClipboard()
                    break
                }

                event.accepted = true
            }
        }
    }
}

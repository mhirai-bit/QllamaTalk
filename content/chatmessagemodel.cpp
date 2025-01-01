#include <stdlib.h>
#include "chatmessagemodel.h"

// Constructor: initializes an empty QAbstractListModel
// コンストラクタ: 空のQAbstractListModelを初期化
ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
}

// Returns how many rows the model contains (number of messages)
// モデルが保持する行数（メッセージ数）を返す
int ChatMessageModel::rowCount(const QModelIndex &) const {
    return static_cast<int>(m_messages.size());
}

// Retrieves data for display based on the requested role
// 指定されたロールに応じて表示用データを返す
QVariant ChatMessageModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(m_messages.size())) {
        return QVariant();
    }

    const auto &data = m_messages.at(row);
    switch (role) {
    case Sender:
        return QVariant::fromValue(QString(data.role));
    case MessageContent:
        return QVariant::fromValue(QString(data.content));
    }

    return QVariant();
}

// Returns role names to map custom roles into QML/views
// QMLや他のビューでカスタムロールをマッピングするためのロール名を返す
QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    static QHash<int, QByteArray> s_roleNames;
    if (s_roleNames.isEmpty()) {
        s_roleNames.insert(Sender,         "sender");
        s_roleNames.insert(MessageContent, "messageContent");
    }
    return s_roleNames;
}

// Appends multiple llama_chat_message objects in one batch
// 複数のllama_chat_messageを一度に追加
void ChatMessageModel::append(const std::vector<llama_chat_message> &messages) {
    if (!messages.empty()) {
        beginInsertRows(QModelIndex(),
                        static_cast<int>(m_messages.size()),
                        static_cast<int>(m_messages.size() + messages.size() - 1));
        m_messages.insert(m_messages.end(), messages.begin(), messages.end());
        endInsertRows();
    }
}

// Appends a single message, returns the new row index
// 単一のメッセージを追加し、新しい行インデックスを返す
int ChatMessageModel::appendSingle(const QString &sender, const QString &content) {
    llama_chat_message msg;

    // Duplicate strings for consistent memory handling
    // 一貫したメモリ管理のため文字列を複製
    msg.role    = strdup(sender.toUtf8().constData());
    msg.content = strdup(content.toUtf8().constData());

    beginInsertRows(QModelIndex(),
                    static_cast<int>(m_messages.size()),
                    static_cast<int>(m_messages.size()));
    m_messages.push_back(msg);
    endInsertRows();

    return static_cast<int>(m_messages.size()) - 1;
}

// Updates the content of an existing message at a specific row
// 指定行の既存メッセージ内容を更新
void ChatMessageModel::updateMessageContent(int row, const QString &newContent) {
    if (row < 0 || row >= static_cast<int>(m_messages.size())) {
        return; // Out of range
    }

    free((void*)m_messages[row].content);
    m_messages[row].content = strdup(newContent.toUtf8().constData());

    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {MessageContent});
}

// Destructor: cleans up allocated memory for each stored llama_chat_message
// デストラクタ: 格納された各llama_chat_messageのメモリを解放
ChatMessageModel::~ChatMessageModel() {
    // Free 'role' and 'content' for all messages
    // 全メッセージのroleとcontentを解放
    for (auto & msg : m_messages) {
        free((void*)msg.role);
        free((void*)msg.content);
    }
    m_messages.clear();
}

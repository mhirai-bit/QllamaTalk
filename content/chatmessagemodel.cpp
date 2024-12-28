#include <stdlib.h>
#include "chatmessagemodel.h"

// This constructor initializes an empty QAbstractListModel.
ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
}

// Destructor cleans up allocated memory for each stored llama_chat_message.
ChatMessageModel::~ChatMessageModel() {
    // Free 'role' and 'content' for all messages.
    for (auto & msg : m_messages) {
        // Casting away const to call free().
        free((void*)msg.role);
        free((void*)msg.content);
    }
    m_messages.clear();
}

// Returns role names to map custom roles into QML or other views.
QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    static QHash<int, QByteArray> s_roleNames;
    if (s_roleNames.isEmpty()) {
        s_roleNames.insert(Sender, "sender");
        s_roleNames.insert(MessageContent, "messageContent");
    }
    return s_roleNames;
}

// Retrieves data for display based on the requested role.
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

// Returns how many rows the model contains (the number of messages).
int ChatMessageModel::rowCount(const QModelIndex &) const {
    return static_cast<int>(m_messages.size());
}

// Appends multiple llama_chat_message objects to the model in one batch.
void ChatMessageModel::append(const std::vector<llama_chat_message> &messages) {
    if (!messages.empty()) {
        beginInsertRows(QModelIndex(),
                        static_cast<int>(m_messages.size()),
                        static_cast<int>(m_messages.size() + messages.size() - 1));
        m_messages.insert(m_messages.end(), messages.begin(), messages.end());
        endInsertRows();
    }
}

// Appends a single message using a sender and content, returns the new row index.
int ChatMessageModel::appendSingle(const QString &sender, const QString &content) {
    llama_chat_message msg;

    // Duplicate strings so they're managed consistently (matching the usage in append()).
    msg.role    = strdup(sender.toUtf8().constData());
    msg.content = strdup(content.toUtf8().constData());

    // Insert one new row at the end of the model.
    beginInsertRows(QModelIndex(),
                    static_cast<int>(m_messages.size()),
                    static_cast<int>(m_messages.size()));
    m_messages.push_back(msg);
    endInsertRows();

    return static_cast<int>(m_messages.size()) - 1;
}

// Updates the content of an existing message at a specific row.
void ChatMessageModel::updateMessageContent(int row, const QString &newContent) {
    if (row < 0 || row >= static_cast<int>(m_messages.size())) {
        return; // Out of valid range
    }

    // Free the old content and assign new content.
    free((void*)m_messages[row].content);
    m_messages[row].content = strdup(newContent.toUtf8().constData());

    // Notify views of data change to refresh the UI.
    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {MessageContent});
}

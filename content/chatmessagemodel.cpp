#include "chatmessagemodel.h"

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    static QHash<int, QByteArray> s_roleNames;
    if (s_roleNames.isEmpty()) {
        s_roleNames.insert(Sender, "sender");
        s_roleNames.insert(MessageContent, "messageContent");
    }
    return s_roleNames;
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const {
    const int row = index.row();

    if (row < 0 || row >= m_messages.size()) {
        return QVariant();
    }

    const auto &data = m_messages.at(row);
    switch (role) {
    case Sender:
        return QVariant::fromValue(data.role);
    case MessageContent:
        return QVariant::fromValue(data.content);
    }

    return QVariant();
}

int ChatMessageModel::rowCount(const QModelIndex &) const {
    return m_messages.size();
}

void ChatMessageModel::append(const std::vector<llama_chat_message> &messages) {
    if (messages.size() > 0) {
        beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size() + messages.size() - 1);
        m_messages.insert(m_messages.end(), messages.begin(), messages.end());
        endInsertRows();
    }
}

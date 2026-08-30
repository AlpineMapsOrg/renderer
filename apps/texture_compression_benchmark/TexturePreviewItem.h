/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#pragma once

#include <QImage>
#include <QQuickFramebufferObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration>
#include <vector>

class QNetworkAccessManager;

class TexturePreviewItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY previewResultsChanged)
    Q_PROPERTY(int previewEncoder READ previewEncoder WRITE setPreviewEncoder NOTIFY previewEncoderChanged)
    Q_PROPERTY(QStringList previewEncoders READ previewEncoders NOTIFY previewResultsChanged)
    Q_PROPERTY(QString previewName READ previewName NOTIFY previewDetailsChanged)
    Q_PROPERTY(QString previewDescription READ previewDescription NOTIFY previewDetailsChanged)
    Q_PROPERTY(double previewPsnr READ previewPsnr NOTIFY previewDetailsChanged)

public:
    struct PreviewResult {
        QString name;
        QString description;
        double psnr = 0.0;
    };

    explicit TexturePreviewItem(QQuickItem* parent = nullptr);
    Renderer* createRenderer() const override;

    [[nodiscard]] QString status() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] bool ready() const;
    [[nodiscard]] int previewEncoder() const;
    void setPreviewEncoder(int value);
    [[nodiscard]] QStringList previewEncoders() const;
    [[nodiscard]] QString previewName() const;
    [[nodiscard]] QString previewDescription() const;
    [[nodiscard]] double previewPsnr() const;

signals:
    void statusChanged();
    void previewEncoderChanged();
    void previewResultsChanged();
    void previewDetailsChanged();

private:
    friend class TexturePreviewRenderer;
    void downloadImages();
    void stitchImages();
    void publishResults(const QString& error, const std::vector<PreviewResult>& results);

    unsigned m_request_serial = 0;
    int m_downloads_remaining = 0;
    QNetworkAccessManager* m_network_manager = nullptr;
    std::vector<QImage> m_downloaded_tiles;
    std::vector<QImage> m_source_images;
    QString m_status = QStringLiteral("Downloading preview imagery…");
    bool m_loading = true;
    int m_preview_encoder = 0;
    std::vector<PreviewResult> m_preview_results;
};

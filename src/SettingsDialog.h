#pragma once

#include <QDialog>
#include <QPoint>
#include <QString>

class QLabel;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QMouseEvent;
class QPushButton;
class QSlider;
class QSpinBox;

class SettingsDialog : public QDialog
{
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    bool logoutRequested() const;
    QString logoutPasswordManagerType() const;
    bool resetRequested() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void chooseBwProgram();
    void chooseProtonPassProgram();
    void requestLogout();
    void requestReset();
    void saveAndAccept();
    void applyDialogStyle();
    void updateOpacityLabel();
    void updateShortcutGuide();
    void updatePasswordManagerControls();
    void applyRecommendedFontForLanguage(const QString& languageCode);

    QComboBox* m_languageCombo = nullptr;
    QComboBox* m_passwordManagerCombo = nullptr;
    QKeySequenceEdit* m_hotkeyEdit = nullptr;
    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QSpinBox* m_resultLimitSpin = nullptr;
    QCheckBox* m_showShortcutHintsCheck = nullptr;
    QComboBox* m_shortcutPresetCombo = nullptr;
    QCheckBox* m_showResultIconsCheck = nullptr;
    QCheckBox* m_fetchFaviconsFromWebCheck = nullptr;
    QCheckBox* m_showResultUsernameCheck = nullptr;
    QCheckBox* m_showResultUriCheck = nullptr;
    QCheckBox* m_showResultTypeCheck = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QCheckBox* m_fontBoldCheck = nullptr;
    QCheckBox* m_customPositionCheck = nullptr;
    QSpinBox* m_positionXSpin = nullptr;
    QSpinBox* m_positionYSpin = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QLabel* m_opacityValue = nullptr;
    QComboBox* m_cornerCombo = nullptr;
    QLabel* m_shortcutsLabel = nullptr;
    QSpinBox* m_reopenRetentionSpin = nullptr;
    QSpinBox* m_autoSyncIntervalSpin = nullptr;
    QCheckBox* m_closeOnFocusLossCheck = nullptr;
    QCheckBox* m_notifyOnCopyCheck = nullptr;
    QCheckBox* m_credentialSessionStorageCheck = nullptr;
    QCheckBox* m_startOnLoginCheck = nullptr;
    QCheckBox* m_autoCheckUpdatesCheck = nullptr;
    QPushButton* m_checkUpdatesButton = nullptr;
    QPushButton* m_logoutButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QLineEdit* m_bwPathEdit = nullptr;
    QLineEdit* m_protonPassPathEdit = nullptr;
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_logoutRequested = false;
    QString m_logoutPasswordManagerType;
    bool m_resetRequested = false;
};

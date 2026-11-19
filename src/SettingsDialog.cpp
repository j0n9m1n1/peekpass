#include "SettingsDialog.h"

#include "AppSettings.h"
#include "I18n.h"
#include "UpdateChecker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtGlobal>

namespace
{
QString uiText(const char* sourceText, const char* koreanFallback = nullptr)
{
    return I18n::translate("SettingsDialog", sourceText, koreanFallback);
}

QString recommendedFontFamily(const QString& languageCode)
{
    const QString normalized = languageCode.trimmed().toLower();
    const QStringList candidates = normalized.startsWith(QStringLiteral("ko"))
        ? QStringList { QStringLiteral("맑은 고딕"), QStringLiteral("Malgun Gothic"), QStringLiteral("Noto Sans CJK KR"), QStringLiteral("Noto Sans KR") }
        : QStringList { QStringLiteral("Segoe UI"), QStringLiteral("Arial") };
    const QStringList families = QFontDatabase::families();

    for (const QString& candidate : candidates) {
        for (const QString& family : families) {
            if (family.compare(candidate, Qt::CaseInsensitive) == 0)
                return family;
        }
    }

    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

int recommendedFontSize(const QString& languageCode)
{
    return languageCode.trimmed().toLower().startsWith(QStringLiteral("ko")) ? 13 : 12;
}
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(uiText("peekpass Settings", "peekpass 설정"));
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    resize(760, 640);
    setMinimumSize(700, 520);

    m_languageCombo = new QComboBox(this);
    const QString currentLanguage = AppSettings::language();
    const QStringList languageCodes = I18n::availableLanguageCodes();
    for (const QString& code : languageCodes)
        m_languageCombo->addItem(I18n::languageName(code), code);
    const int languageIndex = m_languageCombo->findData(currentLanguage);
    m_languageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : m_languageCombo->findData(QStringLiteral("en")));
    m_languageCombo->setMinimumWidth(240);
    m_languageCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(uiText("Dark", "다크"), "dark");
    m_themeCombo->addItem(uiText("Light", "라이트"), "light");
    const int themeIndex = m_themeCombo->findData(AppSettings::theme());
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    m_themeCombo->setMinimumWidth(180);
    m_themeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_passwordManagerCombo = new QComboBox(this);
    m_passwordManagerCombo->addItem(QStringLiteral("Bitwarden"), QStringLiteral("bitwarden"));
    m_passwordManagerCombo->addItem(QStringLiteral("Proton Pass"), QStringLiteral("protonpass"));
    const int passwordManagerIndex = m_passwordManagerCombo->findData(AppSettings::passwordManagerType());
    m_passwordManagerCombo->setCurrentIndex(passwordManagerIndex >= 0 ? passwordManagerIndex : 0);
    m_passwordManagerCombo->setMinimumWidth(180);
    m_passwordManagerCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_hotkeyEdit = new QKeySequenceEdit(AppSettings::showHotkey(), this);
    m_hotkeyEdit->setMaximumSequenceLength(1);
    m_hotkeyEdit->setMinimumWidth(240);
    m_hotkeyEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_shortcutPresetCombo = new QComboBox(this);
    m_shortcutPresetCombo->addItem(uiText("1Password style", "1Password 스타일"), "1password");
    m_shortcutPresetCombo->addItem(uiText("Bitwarden style", "Bitwarden 스타일"), "bitwarden");
    const int shortcutPresetIndex = m_shortcutPresetCombo->findData(AppSettings::shortcutPreset());
    m_shortcutPresetCombo->setCurrentIndex(shortcutPresetIndex >= 0 ? shortcutPresetIndex : 0);
    m_shortcutPresetCombo->setMinimumWidth(240);
    m_shortcutPresetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const auto configureSpinBox = [](QSpinBox* spinBox, int width) {
        spinBox->setFixedWidth(width);
    };

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(360, 1200);
    m_widthSpin->setSuffix(" px");
    m_widthSpin->setValue(AppSettings::windowWidth());
    configureSpinBox(m_widthSpin, 126);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(180, 900);
    m_heightSpin->setSuffix(" px");
    m_heightSpin->setValue(AppSettings::windowHeight());
    configureSpinBox(m_heightSpin, 126);

    m_resultLimitSpin = new QSpinBox(this);
    m_resultLimitSpin->setRange(1, 20);
    m_resultLimitSpin->setSuffix(uiText(" rows", " 개"));
    m_resultLimitSpin->setValue(AppSettings::searchResultLimit());
    configureSpinBox(m_resultLimitSpin, 116);

    m_showShortcutHintsCheck = new QCheckBox(uiText("Shortcut hints", "단축키 안내"), this);
    m_showShortcutHintsCheck->setChecked(AppSettings::showShortcutHints());

    m_showResultIconsCheck = new QCheckBox(uiText("Show icons", "아이콘 표시"), this);
    m_showResultIconsCheck->setChecked(AppSettings::showResultIcons());

    m_fetchFaviconsFromWebCheck = new QCheckBox(uiText("Web favicons", "웹 favicon"), this);
    m_fetchFaviconsFromWebCheck->setChecked(AppSettings::fetchFaviconsFromWeb());

    m_showResultUsernameCheck = new QCheckBox(uiText("Username", "사용자 이름"), this);
    m_showResultUsernameCheck->setChecked(AppSettings::showResultUsername());

    m_showResultUriCheck = new QCheckBox(uiText("Website", "웹 사이트"), this);
    m_showResultUriCheck->setChecked(AppSettings::showResultUri());

    m_showResultTypeCheck = new QCheckBox(uiText("Item type", "항목 유형"), this);
    m_showResultTypeCheck->setChecked(AppSettings::showResultType());

    auto* resultFieldsLayout = new QHBoxLayout;
    resultFieldsLayout->setContentsMargins(0, 0, 0, 0);
    resultFieldsLayout->setSpacing(12);
    resultFieldsLayout->addWidget(m_showResultUsernameCheck);
    resultFieldsLayout->addWidget(m_showResultUriCheck);
    resultFieldsLayout->addWidget(m_showResultTypeCheck);
    resultFieldsLayout->addStretch(1);

    m_fontCombo = new QFontComboBox(this);
    m_fontCombo->setCurrentFont(QFont(AppSettings::quickFontFamily()));
    m_fontCombo->setMinimumWidth(220);
    m_fontCombo->setFixedWidth(240);

    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(9, 22);
    m_fontSizeSpin->setSuffix(" pt");
    m_fontSizeSpin->setValue(AppSettings::quickFontSize());
    configureSpinBox(m_fontSizeSpin, 96);

    m_fontBoldCheck = new QCheckBox(uiText("Bold", "굵게"), this);
    m_fontBoldCheck->setChecked(AppSettings::quickFontBold());

    auto* fontLayout = new QHBoxLayout;
    fontLayout->setContentsMargins(0, 0, 0, 0);
    fontLayout->setSpacing(8);
    fontLayout->addWidget(m_fontCombo);
    fontLayout->addWidget(m_fontSizeSpin);
    fontLayout->addWidget(m_fontBoldCheck);
    fontLayout->addStretch(1);

    m_customPositionCheck = new QCheckBox(uiText("Custom", "직접 지정"), this);
    m_customPositionCheck->setChecked(AppSettings::customWindowPositionEnabled());

    m_positionXSpin = new QSpinBox(this);
    m_positionXSpin->setRange(-20000, 20000);
    m_positionXSpin->setPrefix("X ");
    m_positionXSpin->setSuffix(" px");
    m_positionXSpin->setValue(AppSettings::windowPositionX());
    configureSpinBox(m_positionXSpin, 112);

    m_positionYSpin = new QSpinBox(this);
    m_positionYSpin->setRange(-20000, 20000);
    m_positionYSpin->setPrefix("Y ");
    m_positionYSpin->setSuffix(" px");
    m_positionYSpin->setValue(AppSettings::windowPositionY());
    configureSpinBox(m_positionYSpin, 112);

    auto* positionLayout = new QHBoxLayout;
    positionLayout->setContentsMargins(0, 0, 0, 0);
    positionLayout->setSpacing(8);
    positionLayout->addWidget(m_customPositionCheck);
    positionLayout->addWidget(m_positionXSpin);
    positionLayout->addWidget(m_positionYSpin);
    positionLayout->addStretch(1);

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(70, 100);
    m_opacitySlider->setValue(qRound(AppSettings::windowOpacity() * 100.0));
    m_opacitySlider->setFixedWidth(260);

    m_opacityValue = new QLabel(this);
    m_opacityValue->setFixedWidth(42);
    updateOpacityLabel();

    auto* opacityLayout = new QHBoxLayout;
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacityLayout->setSpacing(8);
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityValue);
    opacityLayout->addStretch(1);

    m_cornerCombo = new QComboBox(this);
    m_cornerCombo->addItem(uiText("Square", "사각"), false);
    m_cornerCombo->addItem(uiText("Rounded", "둥근 모서리"), true);
    m_cornerCombo->setCurrentIndex(AppSettings::roundedCorners() ? 1 : 0);
    m_cornerCombo->setFixedWidth(120);

    auto* shortcutsTitle = new QLabel(uiText("Shortcuts", "단축키"), this);
    shortcutsTitle->setObjectName("settingsSectionTitle");

    m_shortcutsLabel = new QLabel(this);
    m_shortcutsLabel->setObjectName("settingsShortcutText");
    m_shortcutsLabel->setWordWrap(true);
    m_shortcutsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_reopenRetentionSpin = new QSpinBox(this);
    m_reopenRetentionSpin->setRange(0, 120);
    m_reopenRetentionSpin->setSuffix(uiText(" sec", " 초"));
    m_reopenRetentionSpin->setSpecialValueText(uiText("Disabled", "사용 안 함"));
    m_reopenRetentionSpin->setValue(AppSettings::reopenRetentionSeconds());
    configureSpinBox(m_reopenRetentionSpin, 124);

    m_autoSyncIntervalSpin = new QSpinBox(this);
    m_autoSyncIntervalSpin->setRange(0, 1440);
    m_autoSyncIntervalSpin->setSuffix(uiText(" min", " 분"));
    m_autoSyncIntervalSpin->setSpecialValueText(uiText("Disabled", "사용 안 함"));
    m_autoSyncIntervalSpin->setValue(AppSettings::autoSyncIntervalMinutes());
    configureSpinBox(m_autoSyncIntervalSpin, 124);

    m_closeOnFocusLossCheck = new QCheckBox(uiText("Close on focus loss", "포커스 해제 시 닫기"), this);
    m_closeOnFocusLossCheck->setChecked(AppSettings::closeOnFocusLoss());

    m_notifyOnCopyCheck = new QCheckBox(uiText("Show notification after copying", "복사 완료 알림 표시"), this);
    m_notifyOnCopyCheck->setChecked(AppSettings::notifyOnCopyEnabled());

    m_credentialSessionStorageCheck = new QCheckBox(
        uiText("Secure session storage", "보안 저장소에 세션 저장"),
        this);
    m_credentialSessionStorageCheck->setChecked(AppSettings::credentialSessionStorageEnabled());

    m_startOnLoginCheck = new QCheckBox(uiText("Start with Windows", "Windows 자동 시작"), this);
    m_startOnLoginCheck->setChecked(AppSettings::startOnLoginEnabled());
    m_startOnLoginCheck->setToolTip(
        uiText("Registers the deployed peekpass executable for the current Windows user. Bare development builds without Qt runtime files cannot be registered.",
               "배포된 peekpass 실행 파일을 현재 Windows 사용자에 등록합니다. Qt 런타임 파일이 없는 개발 빌드는 등록할 수 없습니다."));

    m_autoCheckUpdatesCheck = new QCheckBox(
        uiText("Check for updates automatically on startup", "시작 시 자동으로 업데이트 확인"),
        this);
    m_autoCheckUpdatesCheck->setChecked(AppSettings::autoCheckUpdatesEnabled());

    m_checkUpdatesButton = new QPushButton(
        uiText("Check for updates...", "업데이트 확인..."),
        this);

    m_logoutButton = new QPushButton(uiText("Log out of Bitwarden...", "Bitwarden 로그아웃..."), this);
    m_logoutButton->setObjectName("dangerButton");
    m_logoutButton->setToolTip(
        uiText("Run bw logout and remove the stored peekpass session.",
               "bw logout을 실행하고 저장된 peekpass 세션을 제거합니다."));

    m_resetButton = new QPushButton(uiText("Reset peekpass...", "peekpass 초기화..."), this);
    m_resetButton->setObjectName("dangerButton");
    m_resetButton->setToolTip(
        uiText("Log out installed password-manager CLIs and clear all local peekpass data.",
               "설치된 비밀번호 관리자 CLI에서 로그아웃하고 모든 로컬 peekpass 데이터를 지웁니다."));

    m_bwPathEdit = new QLineEdit(AppSettings::bwProgramOverride(), this);
    m_bwPathEdit->setMinimumWidth(120);
    m_bwPathEdit->setPlaceholderText(
        uiText("Auto: app folder bw.exe, then PATH bw", "자동: 앱 폴더의 bw.exe, 그 다음 PATH의 bw"));

    auto* browseButton = new QPushButton(uiText("Browse...", "찾기..."), this);
    auto* autoButton = new QPushButton(uiText("Auto", "자동"), this);

    auto* bwLayout = new QHBoxLayout;
    bwLayout->setContentsMargins(0, 0, 0, 0);
    bwLayout->addWidget(m_bwPathEdit, 1);
    bwLayout->addWidget(browseButton);
    bwLayout->addWidget(autoButton);

    m_protonPassPathEdit = new QLineEdit(AppSettings::protonPassProgramOverride(), this);
    m_protonPassPathEdit->setMinimumWidth(120);
    m_protonPassPathEdit->setPlaceholderText(
        uiText("Auto: app folder, standard Proton Pass install, then PATH pass-cli",
               "자동: 앱 폴더, Proton Pass 기본 설치 경로, 그 다음 PATH의 pass-cli"));

    auto* protonBrowseButton = new QPushButton(uiText("Browse...", "찾기..."), this);
    auto* protonAutoButton = new QPushButton(uiText("Auto", "자동"), this);

    auto* protonPassLayout = new QHBoxLayout;
    protonPassLayout->setContentsMargins(0, 0, 0, 0);
    protonPassLayout->addWidget(m_protonPassPathEdit, 1);
    protonPassLayout->addWidget(protonBrowseButton);
    protonPassLayout->addWidget(protonAutoButton);

    auto* sizeLayout = new QHBoxLayout;
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->setSpacing(8);
    sizeLayout->addWidget(m_widthSpin);
    sizeLayout->addWidget(m_heightSpin);
    sizeLayout->addStretch(1);

    auto* searchDisplayLayout = new QGridLayout;
    searchDisplayLayout->setContentsMargins(0, 0, 0, 0);
    searchDisplayLayout->setHorizontalSpacing(10);
    searchDisplayLayout->setVerticalSpacing(4);
    searchDisplayLayout->addWidget(m_resultLimitSpin, 0, 0);
    searchDisplayLayout->addWidget(m_showShortcutHintsCheck, 0, 1);
    searchDisplayLayout->addWidget(m_showResultIconsCheck, 1, 0);
    searchDisplayLayout->addWidget(m_fetchFaviconsFromWebCheck, 1, 1);
    searchDisplayLayout->setColumnStretch(1, 1);

    auto* note = new QLabel(
        uiText("Leave bw.exe empty to use bw.exe beside peekpass first, then bw from PATH. Session storage does not store the master password.",
               "비워두면 peekpass 실행 파일 옆의 bw.exe를 먼저 사용하고, 없으면 PATH의 bw를 사용합니다. 세션 저장은 마스터 비밀번호를 저장하지 않습니다."),
        this);
    note->setWordWrap(true);
    note->setObjectName("settingsNote");

    auto* cliDocs = new QLabel(
        QString("%1 <a href=\"https://bitwarden.com/help/cli/\">https://bitwarden.com/help/cli/</a>")
            .arg(uiText("Bitwarden CLI download and setup:", "Bitwarden CLI 다운로드 및 설정:")),
        this);
    cliDocs->setObjectName("settingsLink");
    cliDocs->setTextFormat(Qt::RichText);
    cliDocs->setTextInteractionFlags(Qt::TextBrowserInteraction);
    cliDocs->setOpenExternalLinks(true);
    cliDocs->setWordWrap(true);

    auto* protonNote = new QLabel(
        uiText("Leave pass-cli.exe empty to use the copy beside peekpass first, then the standard Proton Pass install, then pass-cli from PATH. Proton Pass CLI keeps its own local session.",
               "비워두면 peekpass 실행 파일 옆의 pass-cli.exe, Proton Pass 기본 설치 경로, PATH의 pass-cli 순서로 사용합니다. Proton Pass CLI는 자체 로컬 세션을 관리합니다."),
        this);
    protonNote->setWordWrap(true);
    protonNote->setObjectName("settingsNote");

    auto* protonCliDocs = new QLabel(
        QString("%1 <a href=\"https://protonpass.github.io/pass-cli/get-started/installation/\">https://protonpass.github.io/pass-cli/get-started/installation/</a>")
            .arg(uiText("Proton Pass CLI installation:", "Proton Pass CLI 설치:")),
        this);
    protonCliDocs->setObjectName("settingsLink");
    protonCliDocs->setTextFormat(Qt::RichText);
    protonCliDocs->setTextInteractionFlags(Qt::TextBrowserInteraction);
    protonCliDocs->setOpenExternalLinks(true);
    protonCliDocs->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (QPushButton* okButton = buttons->button(QDialogButtonBox::Ok))
        okButton->setText(uiText("OK", "확인"));
    if (QPushButton* cancelButton = buttons->button(QDialogButtonBox::Cancel))
        cancelButton->setText(uiText("Cancel", "취소"));

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("settingsTabs");

    const auto makeTab = [this, tabs](const QString& title) {
        auto* scrollArea = new QScrollArea(this);
        scrollArea->setObjectName("settingsScrollArea");
        scrollArea->setWidgetResizable(true);

        auto* page = new QWidget(scrollArea);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(12, 10, 12, 10);
        pageLayout->setSpacing(10);

        scrollArea->setWidget(page);
        tabs->addTab(scrollArea, title);
        return pageLayout;
    };

    const auto makeGroup = [this](const QString& title, QVBoxLayout* parentLayout) {
        auto* group = new QGroupBox(title, this);
        group->setObjectName("settingsGroup");

        auto* groupForm = new QFormLayout(group);
        groupForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        groupForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
        groupForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        groupForm->setFormAlignment(Qt::AlignTop);
        groupForm->setHorizontalSpacing(16);
        groupForm->setVerticalSpacing(8);

        parentLayout->addWidget(group);
        return groupForm;
    };

    auto* generalTab = makeTab(uiText("General", "일반"));
    auto* generalForm = makeGroup(uiText("General", "일반"), generalTab);
    generalForm->addRow(uiText("Language", "언어"), m_languageCombo);
    generalForm->addRow(uiText("Theme", "테마"), m_themeCombo);
    generalForm->addRow(uiText("Quick access hotkey", "빠른 액세스 단축키"), m_hotkeyEdit);
    generalForm->addRow(uiText("Shortcut set", "단축키 세트"), m_shortcutPresetCombo);

    generalForm->addRow(uiText("Password manager", "비밀번호 관리자"), m_passwordManagerCombo);

    auto* shortcutPanel = new QGroupBox(this);
    shortcutPanel->setObjectName("settingsGroup");
    auto* shortcutColumn = new QVBoxLayout(shortcutPanel);
    shortcutColumn->setContentsMargins(12, 8, 12, 10);
    shortcutColumn->setSpacing(6);
    shortcutColumn->addWidget(shortcutsTitle);
    shortcutColumn->addWidget(m_shortcutsLabel);
    shortcutColumn->addStretch(1);
    generalTab->addWidget(shortcutPanel);
    generalTab->addStretch(1);

    auto* quickWindowTab = makeTab(uiText("Quick Window", "빠른 창"));
    auto* displayForm = makeGroup(uiText("Search and Display", "검색/표시"), quickWindowTab);
    displayForm->addRow(uiText("Window size", "창 크기"), sizeLayout);
    displayForm->addRow(uiText("Search/display", "검색/표시"), searchDisplayLayout);
    displayForm->addRow(uiText("Result row fields", "결과 행 표시"), resultFieldsLayout);

    auto* appearanceForm = makeGroup(uiText("Appearance", "모양"), quickWindowTab);
    appearanceForm->addRow(uiText("Quick window font", "빠른 창 폰트"), fontLayout);
    appearanceForm->addRow(uiText("Window position", "창 위치"), positionLayout);
    appearanceForm->addRow(uiText("Opacity", "투명도"), opacityLayout);
    appearanceForm->addRow(uiText("Corners", "모서리"), m_cornerCombo);
    quickWindowTab->addStretch(1);

    auto* behaviorTab = makeTab(uiText("Behavior", "동작"));
    auto* behaviorForm = makeGroup(uiText("Behavior", "동작"), behaviorTab);
    behaviorForm->addRow(uiText("Keep search", "검색 유지"), m_reopenRetentionSpin);
    behaviorForm->addRow(uiText("Auto sync", "자동 동기화"), m_autoSyncIntervalSpin);
    behaviorForm->addRow(m_closeOnFocusLossCheck);
    behaviorForm->addRow(m_notifyOnCopyCheck);

    auto* securityForm = makeGroup(uiText("Security and Startup", "보안/시작"), behaviorTab);
    securityForm->addRow(m_credentialSessionStorageCheck);
    securityForm->addRow(m_startOnLoginCheck);
    securityForm->addRow(m_autoCheckUpdatesCheck);

    auto* updateRowLayout = new QHBoxLayout;
    updateRowLayout->setContentsMargins(0, 0, 0, 0);
    updateRowLayout->setSpacing(8);
    auto* versionLabel = new QLabel(
        uiText("Current version: v%1", "현재 버전: v%1").arg(QCoreApplication::applicationVersion()),
        this);
    versionLabel->setObjectName("settingsNote");
    updateRowLayout->addWidget(versionLabel);
    updateRowLayout->addStretch(1);
    updateRowLayout->addWidget(m_checkUpdatesButton);
    securityForm->addRow(updateRowLayout);

    securityForm->addRow(m_logoutButton);
    securityForm->addRow(m_resetButton);
    behaviorTab->addStretch(1);

    auto* bitwardenTab = makeTab(uiText("Bitwarden CLI", "Bitwarden CLI"));
    auto* bitwardenForm = makeGroup(uiText("Bitwarden CLI", "Bitwarden CLI"), bitwardenTab);
    bitwardenForm->addRow(uiText("bw.exe location", "bw.exe 위치"), bwLayout);
    bitwardenTab->addWidget(note);
    bitwardenTab->addWidget(cliDocs);
    bitwardenTab->addStretch(1);

    auto* protonPassTab = makeTab(uiText("Proton Pass CLI", "Proton Pass CLI"));
    auto* protonPassForm = makeGroup(uiText("Proton Pass CLI", "Proton Pass CLI"), protonPassTab);
    protonPassForm->addRow(uiText("pass-cli location", "pass-cli 위치"), protonPassLayout);
    protonPassTab->addWidget(protonNote);
    protonPassTab->addWidget(protonCliDocs);
    protonPassTab->addStretch(1);

    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setObjectName("settingsDialogTitle");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addWidget(titleLabel);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    applyDialogStyle();

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this] {
        updateOpacityLabel();
    });

    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this] {
        applyRecommendedFontForLanguage(m_languageCombo->currentData().toString());
        updateShortcutGuide();
    });

    connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this] {
        applyDialogStyle();
    });

    connect(m_passwordManagerCombo, &QComboBox::currentIndexChanged, this, [this] {
        updatePasswordManagerControls();
    });

    connect(m_customPositionCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_positionXSpin->setEnabled(enabled);
        m_positionYSpin->setEnabled(enabled);
    });
    m_positionXSpin->setEnabled(m_customPositionCheck->isChecked());
    m_positionYSpin->setEnabled(m_customPositionCheck->isChecked());

    connect(m_showResultIconsCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        m_fetchFaviconsFromWebCheck->setEnabled(enabled);
    });
    m_fetchFaviconsFromWebCheck->setEnabled(m_showResultIconsCheck->isChecked());

    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, this, [this] {
        updateShortcutGuide();
    });

    connect(m_shortcutPresetCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateShortcutGuide();
    });

    connect(browseButton, &QPushButton::clicked, this, [this] {
        chooseBwProgram();
    });

    connect(autoButton, &QPushButton::clicked, this, [this] {
        m_bwPathEdit->clear();
    });

    connect(protonBrowseButton, &QPushButton::clicked, this, [this] {
        chooseProtonPassProgram();
    });

    connect(protonAutoButton, &QPushButton::clicked, this, [this] {
        m_protonPassPathEdit->clear();
    });

    connect(m_checkUpdatesButton, &QPushButton::clicked, this, [this] {
        UpdateChecker::instance().checkForUpdates(true, this);
    });
    connect(m_logoutButton, &QPushButton::clicked, this, [this] {
        requestLogout();
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this] {
        requestReset();
    });

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        saveAndAccept();
    });

    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        reject();
    });

    updatePasswordManagerControls();
    updateShortcutGuide();
}

bool SettingsDialog::logoutRequested() const
{
    return m_logoutRequested;
}

QString SettingsDialog::logoutPasswordManagerType() const
{
    return m_logoutPasswordManagerType;
}

bool SettingsDialog::resetRequested() const
{
    return m_resetRequested;
}

void SettingsDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && event->buttons().testFlag(Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void SettingsDialog::chooseBwProgram()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        uiText("Select Bitwarden CLI", "Bitwarden CLI 선택"),
        m_bwPathEdit->text(),
        uiText("Bitwarden CLI (bw.exe bw);;All files (*)", "Bitwarden CLI (bw.exe bw);;모든 파일 (*)"));

    if (!path.isEmpty())
        m_bwPathEdit->setText(path);
}

void SettingsDialog::chooseProtonPassProgram()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        uiText("Select Proton Pass CLI", "Proton Pass CLI 선택"),
        m_protonPassPathEdit->text(),
        uiText("Proton Pass CLI (pass-cli.exe pass-cli);;All files (*)", "Proton Pass CLI (pass-cli.exe pass-cli);;모든 파일 (*)"));

    if (!path.isEmpty())
        m_protonPassPathEdit->setText(path);
}

void SettingsDialog::requestLogout()
{
    const QString managerType = m_passwordManagerCombo->currentData().toString();
    if (managerType == QStringLiteral("protonpass")) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            QStringLiteral("Proton Pass"),
            uiText("This will run pass-cli logout and clear cached vault data. Continue?",
                   "pass-cli logout을 실행하고 캐시된 보관함 데이터를 비웁니다. 계속할까요?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer == QMessageBox::Yes) {
            m_logoutRequested = true;
            m_logoutPasswordManagerType = managerType;
            accept();
        }
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        uiText("Log out of Bitwarden", "Bitwarden 로그아웃"),
        uiText("This will run bw logout, clear cached vault data, and remove the stored peekpass session. Continue?",
               "bw logout을 실행하고, 캐시된 보관함 데이터를 비우고, 저장된 peekpass 세션을 제거합니다. 계속할까요?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (answer != QMessageBox::Yes)
        return;

    m_logoutRequested = true;
    m_logoutPasswordManagerType = managerType;
    accept();
}

void SettingsDialog::requestReset()
{
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this,
        uiText("Reset peekpass", "peekpass 초기화"),
        uiText("This logs out installed Bitwarden and Proton Pass CLIs, clears local peekpass settings, cached data, Windows secure sessions, and Windows startup registration. The app will close and show its first-run state when opened again. Continue?",
               "설치된 Bitwarden 및 Proton Pass CLI에서 로그아웃하고, 로컬 peekpass 설정, 캐시 데이터, Windows 보안 세션, Windows 자동 시작 항목을 지웁니다. 앱은 종료되며 다음 실행에서 첫 화면이 표시됩니다. 계속할까요?"),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    m_resetRequested = true;
    accept();
}

void SettingsDialog::saveAndAccept()
{
    if (m_hotkeyEdit->keySequence().isEmpty()) {
        QMessageBox::warning(
            this,
            uiText("Hotkey", "단축키"),
            uiText("Choose a quick access hotkey first.", "빠른 액세스 단축키를 먼저 선택하세요."));
        return;
    }

    const bool startOnLogin = m_startOnLoginCheck->isChecked();
    if (startOnLogin != AppSettings::startOnLoginEnabled()) {
        QString startupError;
        if (!AppSettings::setStartOnLoginEnabled(startOnLogin, &startupError)) {
            QMessageBox::warning(
                this,
                uiText("Windows startup", "Windows 자동 시작"),
                QString("%1\n\n%2")
                    .arg(uiText("Could not update Windows startup registration.",
                             "Windows 자동 시작 등록을 변경하지 못했습니다."),
                        startupError));
            m_startOnLoginCheck->setChecked(AppSettings::startOnLoginEnabled());
            return;
        }
    }

    AppSettings::setLanguage(m_languageCombo->currentData().toString());
    AppSettings::setTheme(m_themeCombo->currentData().toString());
    AppSettings::setPasswordManagerType(m_passwordManagerCombo->currentData().toString());
    AppSettings::setShowHotkey(m_hotkeyEdit->keySequence());
    AppSettings::setShortcutPreset(m_shortcutPresetCombo->currentData().toString());
    AppSettings::setWindowWidth(m_widthSpin->value());
    AppSettings::setWindowHeight(m_heightSpin->value());
    AppSettings::setSearchResultLimit(m_resultLimitSpin->value());
    AppSettings::setShowShortcutHints(m_showShortcutHintsCheck->isChecked());
    AppSettings::setShowResultIcons(m_showResultIconsCheck->isChecked());
    AppSettings::setFetchFaviconsFromWeb(m_fetchFaviconsFromWebCheck->isChecked());
    AppSettings::setShowResultUsername(m_showResultUsernameCheck->isChecked());
    AppSettings::setShowResultUri(m_showResultUriCheck->isChecked());
    AppSettings::setShowResultType(m_showResultTypeCheck->isChecked());
    AppSettings::setQuickFontFamily(m_fontCombo->currentFont().family());
    AppSettings::setQuickFontSize(m_fontSizeSpin->value());
    AppSettings::setQuickFontBold(m_fontBoldCheck->isChecked());
    AppSettings::setCustomWindowPositionEnabled(m_customPositionCheck->isChecked());
    AppSettings::setWindowPositionX(m_positionXSpin->value());
    AppSettings::setWindowPositionY(m_positionYSpin->value());
    AppSettings::setWindowOpacity(m_opacitySlider->value() / 100.0);
    AppSettings::setRoundedCorners(m_cornerCombo->currentData().toBool());
    AppSettings::setReopenRetentionSeconds(m_reopenRetentionSpin->value());
    AppSettings::setAutoSyncIntervalMinutes(m_autoSyncIntervalSpin->value());
    AppSettings::setCloseOnFocusLoss(m_closeOnFocusLossCheck->isChecked());
    AppSettings::setNotifyOnCopyEnabled(m_notifyOnCopyCheck->isChecked());
    AppSettings::setCredentialSessionStorageEnabled(m_credentialSessionStorageCheck->isChecked());
    AppSettings::setAutoCheckUpdatesEnabled(m_autoCheckUpdatesCheck->isChecked());
    AppSettings::setBwProgramOverride(m_bwPathEdit->text());
    AppSettings::setProtonPassProgramOverride(m_protonPassPathEdit->text());
    accept();
}

void SettingsDialog::updatePasswordManagerControls()
{
    const bool protonPass = m_passwordManagerCombo
        && m_passwordManagerCombo->currentData().toString() == QStringLiteral("protonpass");

    if (m_credentialSessionStorageCheck) {
        m_credentialSessionStorageCheck->setEnabled(!protonPass);
        m_credentialSessionStorageCheck->setToolTip(protonPass
                ? uiText("Proton Pass CLI manages its own session.", "Proton Pass CLI가 자체 세션을 관리합니다.")
                : QString());
    }

    if (m_logoutButton) {
        m_logoutButton->setText(protonPass
                ? uiText("Log out of Proton Pass...", "Proton Pass 로그아웃...")
                : uiText("Log out of Bitwarden...", "Bitwarden 로그아웃..."));
        m_logoutButton->setToolTip(protonPass
                ? uiText("Run pass-cli logout and clear cached vault data.", "pass-cli logout을 실행하고 캐시된 보관함 데이터를 비웁니다.")
                : uiText("Run bw logout and remove the stored peekpass session.", "bw logout을 실행하고 저장된 peekpass 세션을 제거합니다."));
    }
}

void SettingsDialog::applyDialogStyle()
{
    const QString selectedTheme = m_themeCombo ? m_themeCombo->currentData().toString() : AppSettings::theme();
    const bool lightTheme = selectedTheme == QStringLiteral("light");

    if (lightTheme) {
        setStyleSheet(R"(
            QDialog {
                background: #f6f8fa;
                border: 1px solid #d0d7de;
            }
            QLabel {
                color: #24292f;
            }
            #settingsDialogTitle {
                color: #111827;
                font-size: 14px;
                font-weight: 800;
                padding: 1px 2px 5px 2px;
            }
            #settingsNote, #settingsLink {
                color: #57606a;
                font-size: 12px;
            }
            #settingsLink a {
                color: #0969da;
                text-decoration: none;
            }
            #settingsTabs::pane {
                border: 1px solid #d0d7de;
                border-radius: 8px;
                background: #ffffff;
            }
            #settingsTabs QTabBar::tab {
                background: #eef1f4;
                color: #57606a;
                border: 1px solid #d0d7de;
                border-bottom: 0;
                border-top-left-radius: 6px;
                border-top-right-radius: 6px;
                padding: 7px 14px;
                margin-right: 3px;
            }
            #settingsTabs QTabBar::tab:selected {
                background: #ffffff;
                color: #111827;
                border-color: #afb8c1;
            }
            #settingsTabs QTabBar::tab:hover {
                background: #eaeef2;
            }
            #settingsScrollArea {
                background: transparent;
                border: 0;
            }
            #settingsScrollArea > QWidget > QWidget {
                background: transparent;
            }
            #settingsGroup {
                background: #ffffff;
                border: 1px solid #d0d7de;
                border-radius: 8px;
                margin-top: 12px;
                padding: 10px 12px 12px 12px;
            }
            #settingsGroup::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 6px;
                color: #111827;
                font-size: 13px;
                font-weight: 800;
            }
            #settingsSectionTitle {
                color: #111827;
                font-size: 13px;
                font-weight: 800;
            }
            #settingsShortcutText {
                color: #57606a;
                font-size: 11px;
                line-height: 140%;
            }
            QLineEdit, QKeySequenceEdit, QComboBox, QSpinBox {
                background: #ffffff;
                color: #24292f;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 5px 8px;
                selection-background-color: #0969da;
                selection-color: #ffffff;
            }
            QComboBox {
                padding-right: 26px;
            }
            QComboBox::drop-down {
                subcontrol-origin: padding;
                subcontrol-position: top right;
                width: 22px;
                border-left: 1px solid #e5e7eb;
                border-top-right-radius: 6px;
                border-bottom-right-radius: 6px;
                background: transparent;
            }
            QComboBox::down-arrow {
                image: url(:/icons/chevron-down-light.png);
                width: 12px;
                height: 8px;
            }
            QComboBox QAbstractItemView {
                background: #ffffff;
                color: #24292f;
                border: 1px solid #d0d7de;
                selection-background-color: #0969da;
                selection-color: #ffffff;
                outline: 0;
            }
            QSpinBox {
                padding-right: 20px;
            }
            QSpinBox::up-button, QSpinBox::down-button {
                subcontrol-origin: border;
                width: 18px;
                border-left: 1px solid #e5e7eb;
                background: transparent;
            }
            QSpinBox::up-button {
                subcontrol-position: top right;
                border-top-right-radius: 6px;
            }
            QSpinBox::down-button {
                subcontrol-position: bottom right;
                border-top: 1px solid #e5e7eb;
                border-bottom-right-radius: 6px;
            }
            QSpinBox::up-arrow {
                image: url(:/icons/chevron-up-light.png);
                width: 10px;
                height: 6px;
            }
            QSpinBox::down-arrow {
                image: url(:/icons/chevron-down-light.png);
                width: 10px;
                height: 6px;
            }
            QScrollBar:vertical {
                background: transparent;
                width: 8px;
                margin: 2px;
            }
            QScrollBar::handle:vertical {
                background: #c5cbd3;
                min-height: 24px;
                border-radius: 2px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0;
            }
            QCheckBox {
                color: #24292f;
            }
            QPushButton {
                background: #f3f4f6;
                color: #24292f;
                border: 1px solid #d0d7de;
                border-radius: 6px;
                padding: 5px 12px;
            }
            QPushButton:hover {
                background: #eaeef2;
            }
            QPushButton:pressed {
                background: #d8dee4;
            }
            #dangerButton {
                background: #fff1f1;
                color: #b42318;
                border-color: #f0b8b8;
            }
            #dangerButton:hover {
                background: #ffe4e4;
            }
        )");
        return;
    }

    setStyleSheet(R"(
        QDialog {
            background: #202124;
            border: 1px solid #0b0c0f;
        }
        QLabel {
            color: #e8eaed;
        }
        #settingsDialogTitle {
            color: #f8f9fa;
            font-size: 14px;
            font-weight: 800;
            padding: 1px 2px 5px 2px;
        }
        #settingsNote, #settingsLink {
            color: #b7bdc6;
            font-size: 12px;
        }
        #settingsLink a {
            color: #6bb6ff;
            text-decoration: none;
        }
        #settingsTabs::pane {
            border: 1px solid #3a3f46;
            border-radius: 8px;
            background: #181a1f;
        }
        #settingsTabs QTabBar::tab {
            background: #15171a;
            color: #c5cbd3;
            border: 1px solid #3a3f46;
            border-bottom: 0;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 7px 14px;
            margin-right: 3px;
        }
        #settingsTabs QTabBar::tab:selected {
            background: #202124;
            color: #f8f9fa;
            border-color: #58606b;
        }
        #settingsTabs QTabBar::tab:hover {
            background: #252a30;
        }
        #settingsScrollArea {
            background: transparent;
            border: 0;
        }
        #settingsScrollArea > QWidget > QWidget {
            background: transparent;
        }
        #settingsGroup {
            background: #15171a;
            border: 1px solid #3a3f46;
            border-radius: 8px;
            margin-top: 12px;
            padding: 10px 12px 12px 12px;
        }
        #settingsGroup::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 6px;
            color: #f8f9fa;
            font-size: 13px;
            font-weight: 800;
        }
        #settingsSectionTitle {
            color: #f8f9fa;
            font-size: 13px;
            font-weight: 800;
        }
        #settingsShortcutText {
            color: #c5cbd3;
            font-size: 11px;
            line-height: 140%;
        }
        QLineEdit, QKeySequenceEdit, QComboBox, QSpinBox {
            background: #111317;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            border-radius: 6px;
            padding: 5px 8px;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
        }
        QComboBox {
            padding-right: 26px;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left: 1px solid #2a2f36;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron-down-dark.png);
            width: 12px;
            height: 8px;
        }
        QComboBox QAbstractItemView {
            background: #15171a;
            color: #f1f3f4;
            border: 1px solid #3a3f46;
            selection-background-color: #1f8cff;
            selection-color: #ffffff;
            outline: 0;
        }
        QSpinBox {
            padding-right: 20px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 18px;
            border-left: 1px solid #2a2f36;
            background: transparent;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 6px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-top: 1px solid #2a2f36;
            border-bottom-right-radius: 6px;
        }
        QSpinBox::up-arrow {
            image: url(:/icons/chevron-up-dark.png);
            width: 10px;
            height: 6px;
        }
        QSpinBox::down-arrow {
            image: url(:/icons/chevron-down-dark.png);
            width: 10px;
            height: 6px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #3a3f46;
            min-height: 24px;
            border-radius: 2px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QCheckBox {
            color: #e8eaed;
        }
        QPushButton {
            background: #2f343a;
            color: #f1f3f4;
            border: 1px solid #4b5563;
            border-radius: 6px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background: #3a4048;
        }
        QPushButton:pressed {
            background: #252a30;
        }
        #dangerButton {
            background: #7f1d1d;
            border-color: #b45353;
        }
        #dangerButton:hover {
            background: #991b1b;
        }
    )");
}

void SettingsDialog::updateOpacityLabel()
{
    m_opacityValue->setText(QString("%1%").arg(m_opacitySlider->value()));
}

void SettingsDialog::applyRecommendedFontForLanguage(const QString& languageCode)
{
    if (!m_fontCombo || !m_fontSizeSpin || !m_fontBoldCheck)
        return;

    m_fontCombo->setCurrentFont(QFont(recommendedFontFamily(languageCode)));
    m_fontSizeSpin->setValue(recommendedFontSize(languageCode));
    m_fontBoldCheck->setChecked(true);
}

void SettingsDialog::updateShortcutGuide()
{
    if (!m_shortcutsLabel)
        return;

    const QString hotkey = m_hotkeyEdit->keySequence().isEmpty()
        ? uiText("Not set", "미지정")
        : m_hotkeyEdit->keySequence().toString(QKeySequence::NativeText);

    const bool bitwardenStyle = m_shortcutPresetCombo
        && m_shortcutPresetCombo->currentData().toString() == QStringLiteral("bitwarden");
    const QString guide = bitwardenStyle
        ? uiText("Shortcut set: Bitwarden\nQuick access: %1\nEnter: Copy password\nCtrl+Enter: Copy username\nAlt+T: Copy TOTP\nCtrl+Shift+O: Details\nCtrl+R: Sync\nCtrl+/: Settings\nEsc: Close",
                 "단축키 세트: Bitwarden\n빠른 액세스: %1\nEnter: 비밀번호 복사\nCtrl+Enter: 사용자 이름 복사\nAlt+T: TOTP 복사\nCtrl+Shift+O: 상세보기\nCtrl+R: 동기화\nCtrl+/: 설정\nEsc: 닫기")
        : uiText("Shortcut set: 1Password\nQuick access: %1\nCtrl+C: Copy username\nCtrl+Shift+C: Copy password\nCtrl+Alt+C: Copy TOTP\nCtrl+Shift+O: Details\nCtrl+R: Sync\nCtrl+/: Settings\nEsc: Close",
                 "단축키 세트: 1Password\n빠른 액세스: %1\nCtrl+C: 사용자 이름 복사\nCtrl+Shift+C: 비밀번호 복사\nCtrl+Alt+C: TOTP 복사\nCtrl+Shift+O: 상세보기\nCtrl+R: 동기화\nCtrl+/: 설정\nEsc: 닫기");

    m_shortcutsLabel->setText(guide.arg(hotkey));
}

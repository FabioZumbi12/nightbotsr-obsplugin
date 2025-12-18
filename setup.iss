; Script do Inno Setup para o plugin Nightbot para OBS Studio
; Para compilar com uma configuração diferente, use: /DBuildConfig=Release
#ifndef BuildConfig
  #define BuildConfig "RelWithDebInfo"
#endif

#pragma message "Inno Setup - Compiling with BuildConfig: " + BuildConfig

; Desenvolvido por FabioZumbi12

[Setup]
; --- Informações Básicas do Aplicativo ---
AppName=Nightbot SR OBS Plugin
AppVersion=1.0.0
AppPublisher=FabioZumbi12
AppPublisherURL=https://github.com/FabioZumbi12/NightbotSR-ObsPlugin
AppSupportURL=https://github.com/FabioZumbi12/NightbotSR-ObsPlugin/issues
AppUpdatesURL=https://github.com/FabioZumbi12/NightbotSR-ObsPlugin/releases

; --- Opções do Instalador ---
; DefaultDirName será sobrescrito pela seção [Code] para o caminho do OBS Studio.
DefaultDirName={autopf}\obs-studio
; O nome do executável final do setup.
OutputDir=build_x64\{#BuildConfig}
OutputBaseFilename=nightbotsr-obs-plugin-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Privilégios de administrador são necessários para escrever na pasta 'Arquivos de Programas'.
PrivilegesRequired=admin
DirExistsWarning=no
UninstallFilesDir={app}\nightbotsr-obs-plugin-uninstaller
UninstallDisplayName=nightbotsr-obs-plugin-uninstaller
SetupIconFile=img\nightbot.ico
UninstallDisplayIcon={uninstallexe}

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.RemoveConfig=Do you also want to remove the additional configuration files?
brazilianportuguese.RemoveConfig=Deseja também remover os arquivos de configuração adicionais?
english.OBSNotFound=OBS Studio installation not found automatically.#13#10You will be able to select the folder manually.
brazilianportuguese.OBSNotFound=A instalação do OBS Studio não foi encontrada automaticamente.#13#10Você poderá selecionar a pasta manualmente.
english.LaunchOBS=Launch OBS Studio now
brazilianportuguese.LaunchOBS=Iniciar OBS Studio agora

[Files]
; Copia a DLL do plugin para a pasta de plugins do OBS.
Source: "build_x64\{#BuildConfig}\nightbotsr-obs-plugin.dll"; DestDir: "{app}\obs-plugins\64bit\"; Flags: ignoreversion
; Source: "build_x64\{#BuildConfig}\nightbotsr-obs-plugin.pdb"; DestDir: "{app}\obs-plugins\64bit\"; Flags: ignoreversion

; Copia a pasta 'data' do plugin para a pasta de dados do OBS.
Source: "data\locale\*.ini"; DestDir: "{app}\data\obs-plugins\nightbotsr-obs-plugin\locale\"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
; Opcional: Oferece a opção de abrir o OBS Studio após a instalação.
Filename: "{app}\bin\64bit\obs64.exe"; Description: "{cm:LaunchOBS}"; Flags: nowait postinstall shellexec

[UninstallDelete]
; Garante que todos os arquivos e pastas sejam removidos na desinstalação.
Type: files; Name: "{app}\obs-plugins\64bit\nightbotsr-obs-plugin.dll"
Type: filesandordirs; Name: "{app}\data\obs-plugins\nightbotsr-obs-plugin"

[Code]
var
  ObsPath: string;

function InitializeSetup(): Boolean;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\OBS Studio', '', ObsPath) then
  begin
  end
  else
  begin
    MsgBox(CustomMessage('OBSNotFound'), mbInformation, MB_OK);
  end;

  Result := True;
end;

procedure InitializeWizard();
begin
  if ObsPath <> '' then
    WizardForm.DirEdit.Text := ObsPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  TargetDir: string;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    TargetDir := ExpandConstant('{userappdata}\obs-studio\plugin_config\nightbotsr-obs-plugin');

    if DirExists(TargetDir) then
    begin
      if MsgBox(
        CustomMessage('RemoveConfig') + #13#10 + TargetDir,
        mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      begin
        DelTree(TargetDir, True, True, True);
      end;
    end;
  end;
end;
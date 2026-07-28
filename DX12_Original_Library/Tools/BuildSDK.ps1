Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

try 
{
    Write-Host "======================================"
    Write-Host " TSGameLib SDK build"
    Write-Host "======================================"
    Write-Host ""

    # ps1を含むディレクトリのルートを取得する
    $scriptDirectory = $PSScriptRoot

    # SDKのToolsからリポジトリへの移動
    $repositoryRelativePath = Join-Path -Path $scriptDirectory -ChildPath ".."
    $resolvedRepositoryPath = Resolve-Path -LiteralPath $repositoryRelativePath
    $repositoryRoot = $resolvedRepositoryPath.Path

    # SDK単体動作確認用プロジェクトの原本
    $smokeTestTemplateDirectory = Join-Path -Path $scriptDirectory -ChildPath "SmokeTestTemplate"
    # テンプレートからコピーするファイルを限定する
    # .vs、obj、vcxproj.userなどはコピーしない
    $smokeTestTemplateFiles = @(
        "main.cpp"
        "TSGameLibSDKSmokeTest.vcxproj"
        "TSGameLibSDKSmokeTest.slnx"
    )

    # SDK生成と検証に使用する各パスを作る
    $sourceRoot = Join-Path -Path $repositoryRoot -ChildPath "Src"
    $projectFile = Join-Path -Path $repositoryRoot -ChildPath "DX12_Original_Library.vcxproj"
    $solutionFile = Join-Path -Path $repositoryRoot -ChildPath "DX12_Original_Library.slnx"
    $directXTexProjectFile = Join-Path -Path $repositoryRoot -ChildPath "Src/External/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj"
    $manifestPath = Join-Path -Path $scriptDirectory -ChildPath "PublicHeaders.txt"
    $sdkTemplateDirectory = Join-Path -Path $scriptDirectory -ChildPath "SDKTemplate"

    # Srcフォルダが存在するか確認する
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container))
    {
        throw "Src directory was not found: $sourceRoot"
    }
    # TSGameLib本体のプロジェクトが存在するか確認する
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf))
    {
        throw "Project file was not found: $projectFile"
    }
    # ソリューションファイルが存在するか確認する
    if (-not (Test-Path -LiteralPath $solutionFile -PathType Leaf))
    {
        throw "Solution file was not found: $solutionFile"
    }
    # DirectXTexのプロジェクトが存在するか確認する
    if (-not (Test-Path -LiteralPath $directXTexProjectFile -PathType Leaf))
    {
        throw "DirectXTex project file was not found: $directXTexProjectFile"
    }
    # 公開ヘッダーマニフェストが存在するか確認する
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf))
    {
        throw "Header manifest was not found: $manifestPath"
    }
    # SDKへそのまま梱包する静的ファイルの原本フォルダを確認する
    if (-not (Test-Path -LiteralPath $sdkTemplateDirectory -PathType Container))
    {
        throw "SDK template directory was not found: $sdkTemplateDirectory"
    }
    # SDKTemplate内に必要な3ファイルが揃っているか確認する
    $templateRequiredFiles = @(
        (Join-Path -Path $sdkTemplateDirectory -ChildPath "TSGameLib.props")
        (Join-Path -Path $sdkTemplateDirectory -ChildPath "licenses/DirectXTex_LICENSE.txt")
        (Join-Path -Path $sdkTemplateDirectory -ChildPath "samples/Minimal/main.cpp")
    )
    # SmokeTestの原本フォルダを確認する
    if (-not (Test-Path -LiteralPath $smokeTestTemplateDirectory -PathType Container))
    {
        throw "SmokeTest template directory was not found: $smokeTestTemplateDirectory"
    }

    # SmokeTestに必要なファイルを確認する
    foreach ($smokeTestTemplateFile in $smokeTestTemplateFiles)
    {
        $smokeTestTemplatePath = Join-Path -Path $smokeTestTemplateDirectory -ChildPath $smokeTestTemplateFile

        if (-not (Test-Path -LiteralPath $smokeTestTemplatePath -PathType Leaf))
        {
            throw "SmokeTest template file was not found: $smokeTestTemplatePath"
        }
    }

    foreach ($templateRequiredFile in $templateRequiredFiles)
    {
        if (-not (Test-Path -LiteralPath $templateRequiredFile -PathType Leaf))
        {
            throw "SDK template file was not found: $templateRequiredFile"
        }
    }

    # マニフェストの各行を取得
    $manifestLines = Get-Content -LiteralPath $manifestPath
    # 格納
    $publicHeaders = @()

     foreach ($manifestLine in $manifestLines)
    {
        # 先頭と末尾の空白を消す
        $relativePath = $manifestLine.Trim()

        # 空白行を無視
        if ([string]::IsNullOrWhiteSpace($relativePath))
        {
            continue
        }

        # コメントの行を無視
        if ($relativePath.StartsWith("#"))
        {
            continue
        }

        $relativePath = $relativePath.Replace("\", "/")

        # マニフェストには絶対パスを書かない
        if ([System.IO.Path]::IsPathRooted($relativePath))
        {
            throw "Absolute paths are not allowed: $relativePath"
        }

        # パスがSrc外に行かないようにする
        $pathParts = $relativePath.Split("/")

        if (($pathParts -contains "..") -or ($pathParts -contains "."))
        {
            throw "Relative traversal is not allowed: $relativePath"
        }

        # 重複拒否
        if ($publicHeaders -contains $relativePath)
        {
            throw "Duplicate header entry: $relativePath"
        }

        # ヘッダがあるかチェック
        $sourceHeaderPath = Join-Path -Path $sourceRoot -ChildPath $relativePath

        if (-not (Test-Path -LiteralPath $sourceHeaderPath -PathType Leaf))
        {
            throw "Public header was not found: $sourceHeaderPath"
        }

        # パスの保存
        $publicHeaders += $relativePath
    }

    if ($publicHeaders.Count -eq 0)
    {
        throw "The public header manifest is empty."
    }

    Write-Host "[PASS] Public header manifest is valid." -ForegroundColor Green
    Write-Host "Header count: $($publicHeaders.Count)"
    Write-Host ""

    foreach ($publicHeader in $publicHeaders)
    {
        Write-Host "  $publicHeader"
    }
    Write-Host ""

    # Visual Studio Installerが用意するvswhere.exeのパスを作る
    $vswherePath = Join-Path -Path ${env:ProgramFiles(x86)} -ChildPath "Microsoft Visual Studio/Installer/vswhere.exe"

    # vswhere.exeが存在しなければVisual Studioを探索できないため停止する
    if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf))
    {
        throw "vswhere.exe was not found: $vswherePath"
    }

    # vswhere.exeを実行し、MSBuildが入っている最新のVisual Studioから
    # MSBuild.exeのパスを取得する
    $msbuildCandidates = & $vswherePath -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"

    # 複数のMSBuildが見つかった場合は、先頭の1件を使用する
    $msbuildPath = $msbuildCandidates |
            Select-Object -First 1

    # 候補が1件もなければMSBuildを実行できないため停止する
    if ([string]::IsNullOrWhiteSpace($msbuildPath))
    {
        throw "MSBuild.exe was not found."
    }

    # vswhere.exeが返したパスに本当にMSBuild.exeが存在するか確認する
    if (-not (Test-Path -LiteralPath $msbuildPath -PathType Leaf))
    {
        throw "Detected MSBuild.exe does not exist: $msbuildPath"
    }

    Write-Host "[PASS] MSBuild.exe was found." -ForegroundColor Green
    Write-Host "MSBuild path: $msbuildPath"

    Write-Host ""

    # 自動ビルドする構成を配列として用意する
    $configurations = @(
        "Debug"
        "Release"
    )

    # Debug、Releaseの順に処理する
    foreach ($configuration in $configurations)
    {
        Write-Host "========================================"
        Write-Host " Building $configuration|x64"
        Write-Host "========================================"

        # MSBuildへ共通して渡す引数を配列にまとめる
        $msbuildArguments = @(
            "/t:Rebuild"
            "/p:Configuration=$configuration"
            "/p:Platform=x64"
            # 空白パス対策
            "/p:SolutionDir=$repositoryRoot\\"
            "/m"
            "/nologo"
            "/verbosity:minimal"
        )

        # TSGameLibが利用するDirectXTexを先にビルドする
        Write-Host ""
        Write-Host "Building DirectXTex..."

        & $msbuildPath $directXTexProjectFile @msbuildArguments

        # 外部プログラムが返した終了コードをすぐに保存する
        $directXTexBuildExitCode = $LASTEXITCODE

        if ($directXTexBuildExitCode -ne 0)
        {
            throw "DirectXTex $configuration build failed. ExitCode=$directXTexBuildExitCode"
        }

        Write-Host "[PASS] DirectXTex $configuration build succeeded." -ForegroundColor Green

        # TSGameLib本体をビルドする
        Write-Host ""
        Write-Host "Building TSGameLib..."

        & $msbuildPath $projectFile @msbuildArguments

        # 外部プログラムが返した終了コードをすぐに保存する
        $tsGameLibBuildExitCode = $LASTEXITCODE

        if ($tsGameLibBuildExitCode -ne 0)
        {
            throw "TSGameLib $configuration build failed. ExitCode=$tsGameLibBuildExitCode"
        }

        Write-Host "[PASS] TSGameLib $configuration build succeeded." `
            -ForegroundColor Green

        # ビルド後に生成される予定のファイルパスを作る
        $tsGameLibOutput = Join-Path -Path $repositoryRoot -ChildPath "x64/$configuration/DX12_Original_Library.lib"

        $directXTexOutputDirectory = Join-Path -Path $repositoryRoot -ChildPath "Src/External/DirectXTex/Bin/Desktop_2022_Win10/x64/$configuration"

        $directXTexLibrary = Join-Path -Path $directXTexOutputDirectory -ChildPath "DirectXTex.lib"

        $directXTexPdb = Join-Path -Path $directXTexOutputDirectory -ChildPath "DirectXTex.pdb"

        # MSBuildが成功を返しても、必要な生成物がなければ失敗にする
        $expectedBuildOutputs = @(
            $tsGameLibOutput
            $directXTexLibrary
            $directXTexPdb
        )

        foreach ($expectedBuildOutput in $expectedBuildOutputs)
        {
            if (-not (Test-Path -LiteralPath $expectedBuildOutput -PathType Leaf))
            {
                throw "Expected build output was not found: $expectedBuildOutput"
            }
        }

        Write-Host "[PASS] $configuration build outputs were found." -ForegroundColor Green

        Write-Host ""
    }

    Write-Host "[PASS] Debug and Release builds succeeded." -ForegroundColor Green

    Write-Host ""
    Write-Host "========================================"
    Write-Host " Creating staging SDK"
    Write-Host "========================================"

    # 正式SDKへ置き換える前の仮生成場所を作る
    $stagingDirectory = Join-Path -Path $repositoryRoot -ChildPath "TSGameLibSDK.staging"
    # 削除対象を取り違えないように、末尾のフォルダ名を確認する
    $stagingDirectoryName = Split-Path -Path $stagingDirectory -Leaf

    if ($stagingDirectoryName -ne "TSGameLibSDK.staging")
    {
        throw "Unsafe staging directory path: $stagingDirectory"
    }

    # 前回の実行で残ったステージングだけを削除する
    if (Test-Path -LiteralPath $stagingDirectory)
    {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
    # 空のステージングフォルダを作る
    New-Item -ItemType Directory -Path $stagingDirectory -Force |Out-Null
    # props、ライセンス、サンプルなどの静的ファイルをコピーする
    $templateItems = Get-ChildItem -LiteralPath $sdkTemplateDirectory -Force
    foreach ($templateItem in $templateItems)
    {
        Copy-Item -LiteralPath $templateItem.FullName -Destination $stagingDirectory -Recurse -Force
    }

    # 公開ヘッダーを配置するincludeルートを作る
    $stagingIncludeRoot = Join-Path -Path $stagingDirectory -ChildPath "include/TSGameLib"
    New-Item -ItemType Directory -Path $stagingIncludeRoot -Force |Out-Null

    # マニフェストで許可された公開ヘッダーだけをコピーする
    foreach ($publicHeader in $publicHeaders)
    {
        $sourceHeaderPath = Join-Path -Path $sourceRoot -ChildPath $publicHeader
        $destinationHeaderPath = Join-Path -Path $stagingIncludeRoot -ChildPath $publicHeader
        $destinationHeaderDirectory = Split-Path -Path $destinationHeaderPath -Parent

        # Collision、Facade、Mathなどの配置先フォルダを作る
        New-Item -ItemType Directory -Path $destinationHeaderDirectory -Force |Out-Null
        Copy-Item -LiteralPath $sourceHeaderPath -Destination $destinationHeaderPath -Force
    }

    # 外部ユーザー向けシェーダー契約書の配置先を作る
    $stagingShaderIncludeDirectory = Join-Path -Path $stagingDirectory -ChildPath "shaders/include"
    New-Item -ItemType Directory -Path $stagingShaderIncludeDirectory -Force |Out-Null

    # シェーダー契約書はSrc/Shadersを原本としてコピーする
    $shaderContractFiles = @(
        "PostEffectContract.hlsli"
        "SpriteContract.hlsli"
    )

    foreach ($shaderContractFile in $shaderContractFiles)
    {
        $sourceShaderContract = Join-Path -Path $repositoryRoot -ChildPath "Src/Shaders/$shaderContractFile"

        if (-not (Test-Path -LiteralPath $sourceShaderContract -PathType Leaf))
        {
            throw "Shader contract was not found: $sourceShaderContract"
        }

        $destinationShaderContract = Join-Path -Path $stagingShaderIncludeDirectory -ChildPath $shaderContractFile
        Copy-Item -LiteralPath $sourceShaderContract -Destination $destinationShaderContract -Force
    }

    # DebugとReleaseのlib・PDBをステージングへコピーする
    foreach ($configuration in $configurations)
    {
        $stagingLibraryDirectory = Join-Path -Path $stagingDirectory -ChildPath "lib/x64/$configuration"
        New-Item -ItemType Directory -Path $stagingLibraryDirectory -Force |Out-Null

        $tsGameLibSourceLibrary = Join-Path -Path $repositoryRoot -ChildPath "x64/$configuration/DX12_Original_Library.lib"

        $directXTexSourceDirectory = Join-Path -Path $repositoryRoot -ChildPath "Src/External/DirectXTex/Bin/Desktop_2022_Win10/x64/$configuration"

        $directXTexSourceLibrary = Join-Path -Path $directXTexSourceDirectory -ChildPath "DirectXTex.lib"

        $directXTexSourcePdb = Join-Path -Path $directXTexSourceDirectory -ChildPath "DirectXTex.pdb"

        Copy-Item -LiteralPath $tsGameLibSourceLibrary -Destination $stagingLibraryDirectory -Force

        Copy-Item -LiteralPath $directXTexSourceLibrary -Destination $stagingLibraryDirectory -Force

        Copy-Item -LiteralPath $directXTexSourcePdb -Destination $stagingLibraryDirectory -Force
    }

    # ステージングへコピーされた公開ヘッダー数を確認する
    $stagedHeaders = @(Get-ChildItem -LiteralPath $stagingIncludeRoot -Recurse -File -Filter "*.h")
    if ($stagedHeaders.Count -ne $publicHeaders.Count)
    {
        throw "Staged header count mismatch. Expected=$($publicHeaders.Count), Actual=$($stagedHeaders.Count)"
    }

    # SDK利用に必要な代表ファイルを検査する
    $stagedRequiredFiles = @(
        (Join-Path -Path $stagingDirectory -ChildPath "TSGameLib.props")
        (Join-Path -Path $stagingDirectory -ChildPath "licenses/DirectXTex_LICENSE.txt")
        (Join-Path -Path $stagingDirectory -ChildPath "samples/Minimal/main.cpp")
        (Join-Path -Path $stagingDirectory -ChildPath "shaders/include/PostEffectContract.hlsli")
        (Join-Path -Path $stagingDirectory -ChildPath "shaders/include/SpriteContract.hlsli")
    )

    foreach ($configuration in $configurations)
    {
        $stagedRequiredFiles += Join-Path -Path $stagingDirectory -ChildPath "lib/x64/$configuration/DX12_Original_Library.lib"

        $stagedRequiredFiles += Join-Path -Path $stagingDirectory -ChildPath "lib/x64/$configuration/DirectXTex.lib"

        $stagedRequiredFiles += Join-Path -Path $stagingDirectory -ChildPath "lib/x64/$configuration/DirectXTex.pdb"
    }

    foreach ($stagedRequiredFile in $stagedRequiredFiles)
    {
        if (-not (Test-Path -LiteralPath $stagedRequiredFile -PathType Leaf))
        {
            throw "Required staged file was not found: $stagedRequiredFile"
        }
    }

    Write-Host "[PASS] Staging SDK was created." -ForegroundColor Green
    Write-Host "Staging path: $stagingDirectory"
    Write-Host "Public headers: $($stagedHeaders.Count)"
    Write-Host ""

    Write-Host "========================================"
    Write-Host " Creating external SmokeTest"
    Write-Host "========================================"

   # 一時フォルダではないユーザー専用のローカルデータ領域を取得する
    $localApplicationDataDirectory = [System.Environment]::GetFolderPath([System.Environment+SpecialFolder]::LocalApplicationData)

    if ([string]::IsNullOrWhiteSpace($localApplicationDataDirectory))
    {
        throw "LocalApplicationData directory could not be resolved."
    }

    # リポジトリ外かつ、一時ファイル扱いされない場所にテスト環境を作る
    $smokeTestRootDirectory = Join-Path -Path $localApplicationDataDirectory -ChildPath "TSGameLibSDKSmokeTest"


    $smokeTestSdkDirectory = Join-Path -Path $smokeTestRootDirectory -ChildPath "SDK"
    $smokeTestProjectDirectory = Join-Path -Path $smokeTestRootDirectory -ChildPath "Test"

    # 削除先を取り違えないよう、フォルダ名を検査する
    $smokeTestRootName = Split-Path -Path $smokeTestRootDirectory -Leaf

    if ($smokeTestRootName -ne "TSGameLibSDKSmokeTest")
    {
        throw "Unsafe SmokeTest directory path: $smokeTestRootDirectory"
    }

        # 前回作ったテスト環境だけを削除する
    if (Test-Path -LiteralPath $smokeTestRootDirectory)
    {
        Remove-Item -LiteralPath $smokeTestRootDirectory -Recurse -Force
    }

    # SDK用とプロジェクト用のフォルダを作る
    New-Item -ItemType Directory -Path $smokeTestSdkDirectory -Force |Out-Null
    New-Item -ItemType Directory -Path $smokeTestProjectDirectory -Force |Out-Null
    # 今回生成したステージングSDKを外部テスト環境へコピーする
    $stagingItems = Get-ChildItem -LiteralPath $stagingDirectory -Force

    foreach ($stagingItem in $stagingItems)
    {
        Copy-Item -LiteralPath $stagingItem.FullName -Destination $smokeTestSdkDirectory -Recurse -Force
    }

    # テストテンプレートは必要な3ファイルだけをコピーする
    # .vs、obj、vcxproj.userなどは持ち込まない
    foreach ($smokeTestTemplateFile in $smokeTestTemplateFiles)
    {
        $smokeTestTemplateSource = Join-Path -Path $smokeTestTemplateDirectory -ChildPath $smokeTestTemplateFile
        Copy-Item -LiteralPath $smokeTestTemplateSource -Destination $smokeTestProjectDirectory -Force
    }

    # SDKとテストプロジェクトが正しい配置になったか確認する
    $externalSmokeTestRequiredFiles = @(
        (Join-Path $smokeTestSdkDirectory "TSGameLib.props")
        (Join-Path $smokeTestProjectDirectory "main.cpp")
        (Join-Path $smokeTestProjectDirectory "TSGameLibSDKSmokeTest.vcxproj")
        (Join-Path $smokeTestProjectDirectory "TSGameLibSDKSmokeTest.slnx")
    )

    foreach ($externalSmokeTestRequiredFile in $externalSmokeTestRequiredFiles)
    {
        if (-not (Test-Path -LiteralPath $externalSmokeTestRequiredFile -PathType Leaf))
        {
            throw "External SmokeTest file was not found: $externalSmokeTestRequiredFile"
        }
    }

    Write-Host "[PASS] External SmokeTest environment was created." -ForegroundColor Green
    Write-Host "SmokeTest path: $smokeTestRootDirectory"
    Write-Host ""

    Write-Host "========================================"
    Write-Host " Building external SmokeTest"
    Write-Host "========================================"
    Write-Host ""

    # リポジトリ外へコピーしたSolutionをビルド対象にする
    # 原本のSmokeTestTemplateを直接ビルドしないことが重要
    $smokeTestSolutionFile = Join-Path -Path $smokeTestProjectDirectory -ChildPath "TSGameLibSDKSmokeTest.slnx"

    if (-not (Test-Path -LiteralPath $smokeTestSolutionFile -PathType Leaf))
    {
        throw "External SmokeTest solution was not found: $smokeTestSolutionFile"
    }

    # Debug、Releaseの両方でSDKが利用できるか確認する
    foreach ($configuration in $configurations)
    {
        Write-Host "Building external SmokeTest $configuration|x64..."

        $smokeTestBuildArguments = @(
            $smokeTestSolutionFile
            "/t:Rebuild"
            "/p:Configuration=$configuration"
            "/p:Platform=x64"
            "/m"
            "/nologo"
            "/verbosity:minimal"
        )

        # 外部フォルダにあるSolutionをMSBuildでビルドする
        & $msbuildPath $smokeTestBuildArguments

        # MSBuildの終了コードは実行直後に確認する
        if ($LASTEXITCODE -ne 0)
        {
            throw "External SmokeTest $configuration build failed. ExitCode=$LASTEXITCODE"
        }

        # vcxprojのOutDir設定から、生成予定のexeパスを作る
        $smokeTestExecutable = Join-Path -Path $smokeTestProjectDirectory -ChildPath "bin/x64/$configuration/TSGameLibSDKSmokeTest.exe"

        # MSBuildが成功しても、exeがなければ失敗とする
        if (-not (Test-Path -LiteralPath $smokeTestExecutable -PathType Leaf))
        {
            throw "SmokeTest executable was not found: $smokeTestExecutable"
        }

        Write-Host "[PASS] External SmokeTest $configuration build succeeded." -ForegroundColor Green

        Write-Host "Executable: $smokeTestExecutable"
        Write-Host ""
    }

    Write-Host "[PASS] External SmokeTest Debug and Release builds succeeded." -ForegroundColor Green

    Write-Host ""

        Write-Host "========================================"
    Write-Host " Running external SmokeTest"
    Write-Host "========================================"
    Write-Host ""

    # 実際の配布に近いRelease版を実行対象にする
    $releaseSmokeTestExecutable = Join-Path -Path $smokeTestProjectDirectory -ChildPath "bin/x64/Release/TSGameLibSDKSmokeTest.exe"

    if (-not (Test-Path -LiteralPath $releaseSmokeTestExecutable -PathType Leaf))
    {
        throw "Release SmokeTest executable was not found: $releaseSmokeTestExecutable"
    }

    # exeの置かれたフォルダを作業フォルダとして起動する
    $releaseSmokeTestWorkingDirectory = Split-Path -Path $releaseSmokeTestExecutable -Parent

    $smokeTestProcess = Start-Process -FilePath $releaseSmokeTestExecutable -WorkingDirectory $releaseSmokeTestWorkingDirectory -PassThru

    # 無限ループや終了処理の不具合に備え、最大15秒だけ待つ
    $smokeTestCompleted = $smokeTestProcess.WaitForExit(15000)

    if (-not $smokeTestCompleted)
    {
        # 制限時間を超えたプロセスを終了させる
        try
        {
            $smokeTestProcess.Kill()
            $smokeTestProcess.WaitForExit()
        }
        catch
        {
            # 終了直前に自然終了した場合などは、Kill失敗を無視する
        }

        throw "Release SmokeTest timed out after 15 seconds."
    }

    # 終了後のプロセス情報を更新する
    $smokeTestProcess.Refresh()
    $smokeTestExitCode = $smokeTestProcess.ExitCode

    # main.cppが返した終了コードを検査する
    if ($smokeTestExitCode -ne 0)
    {
        if ($smokeTestExitCode -eq 1)
        {
            throw "Release SmokeTest failed: TSLib::Initialize returned false."
        }

        if ($smokeTestExitCode -eq 2)
        {
            throw "Release SmokeTest failed: 120 frames were not completed."
        }

        throw "Release SmokeTest failed. ExitCode=$smokeTestExitCode"
    }

    Write-Host "[PASS] Release SmokeTest completed successfully." -ForegroundColor Green

    Write-Host "ExitCode: $smokeTestExitCode"
    Write-Host ""

    Write-Host "========================================"
    Write-Host " Promoting staging SDK"
    Write-Host "========================================"
    Write-Host ""

    # 正式なSDKフォルダと、置き換え中の退避先を作る
    $officialSdkDirectory = Join-Path -Path $repositoryRoot -ChildPath "TSGameLibSDK"

    $backupSdkDirectory = Join-Path -Path $repositoryRoot -ChildPath "TSGameLibSDK.backup"

    # 削除・移動対象を取り違えないよう、フォルダ名を確認する
    if ((Split-Path $officialSdkDirectory -Leaf) -ne "TSGameLibSDK")
    {
        throw "Unsafe official SDK directory path: $officialSdkDirectory"
    }

    if ((Split-Path $backupSdkDirectory -Leaf) -ne "TSGameLibSDK.backup")
    {
        throw "Unsafe SDK backup directory path: $backupSdkDirectory"
    }

    # 前回の処理で退避フォルダが残っていた場合は削除する
    if (Test-Path -LiteralPath $backupSdkDirectory)
    {
        Remove-Item -LiteralPath $backupSdkDirectory -Recurse -Force
    }

    # 現在の正式SDKが存在するか記録する
    $hadExistingOfficialSdk = Test-Path -LiteralPath $officialSdkDirectory -PathType Container

    # 現在の正式SDKを一時的に退避する
    if ($hadExistingOfficialSdk)
    {
        Move-Item -LiteralPath $officialSdkDirectory -Destination $backupSdkDirectory
    }

    try
    {
        # 全試験に合格したステージングを正式SDKへ昇格する
        Move-Item -LiteralPath $stagingDirectory -Destination $officialSdkDirectory

        # 昇格後もpropsが存在することを確認する
        $officialPropsFile = Join-Path -Path $officialSdkDirectory -ChildPath "TSGameLib.props"

        if (-not (Test-Path -LiteralPath $officialPropsFile -PathType Leaf))
        {
            throw "Official SDK props file was not found: $officialPropsFile"
        }

        # 公開ヘッダー数が変わっていないことを確認する
        $officialIncludeRoot = Join-Path -Path $officialSdkDirectory -ChildPath "include/TSGameLib"

        $officialHeaders = @(
            Get-ChildItem -LiteralPath $officialIncludeRoot -Recurse -File -Filter "*.h"
        )

        if ($officialHeaders.Count -ne $publicHeaders.Count)
        {
            throw "Official SDK header count mismatch. Expected=$($publicHeaders.Count), Actual=$($officialHeaders.Count)"
        }
    }
    catch
    {
        # 昇格失敗の理由を退避する
        $promotionErrorMessage = $_.Exception.Message

        # 途中まで作られた正式SDKを削除する
        if (Test-Path -LiteralPath $officialSdkDirectory)
        {
            Remove-Item -LiteralPath $officialSdkDirectory -Recurse -Force
        }

        # 以前の正式SDKがあれば元へ戻す
        if (Test-Path -LiteralPath $backupSdkDirectory)
        {
            Move-Item -LiteralPath $backupSdkDirectory -Destination $officialSdkDirectory
        }

        throw "SDK promotion failed and was rolled back: $promotionErrorMessage"
    }

    # 正式SDKへの昇格が完了したため、古いSDKの退避を削除する
    if (Test-Path -LiteralPath $backupSdkDirectory)
    {
        Remove-Item -LiteralPath $backupSdkDirectory -Recurse -Force
    }

    Write-Host "[PASS] Staging SDK was promoted to the official SDK." -ForegroundColor Green

    Write-Host "Official SDK path: $officialSdkDirectory"
    Write-Host ""

    exit 0
}
catch {
    Write-Host ""
    Write-Host "[ERROR] SDK build failed." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
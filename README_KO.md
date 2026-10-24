# HeelsFixNG — Skyrim SE 1.5.97 / NPC 가구 보정

이 저장소는 Skyrim Special Edition **1.5.97 전용 빌드 대상**인 C++ SKSE 플러그인 소스입니다. 이전에 배포된 PowerShell 패치 전용 묶음과 달리, 수정된 `src/*.cpp` 및 `src/*.h` 파일을 이 저장소에 직접 포함합니다.

## 목표 및 범위

- 하이힐을 신은 NPC가 일반 의자, 왕좌, 바 의자, 작업대·카운터 등의 가구와 상호작용할 때 발·무릎·몸통 위치를 보정합니다.
- RaceMenu **0.4.11의 NiTransform v2 인터페이스**를 대상으로 하는 호환 경로가 포함되어 있습니다.
- 게임 내 설정 페이지는 SKSE Menu Framework API를 사용합니다.
- ESP/ESM/ESL 플러그인, Papyrus 스크립트, SexLab/P+ 연동은 배포 대상에 포함하지 않습니다.
- 플레이어 보정은 기본 비활성화이며 메뉴와 INI에서 켤 수 없도록 제한합니다.
- Skyrim 1.5.97을 다른 게임 런타임으로 변경하지 않습니다.

## 필요한 런타임

- Skyrim SE 1.5.97 및 그 버전에 맞는 SKSE64
- Address Library for SKSE Plugins (해당 게임 버전용)
- RaceMenu 0.4.11
- SKSE Menu Framework

ECE는 캐릭터 편집 환경으로 계속 사용할 수 있습니다. 이 플러그인은 RaceMenu의 NiTransform 인터페이스를 사용하므로 RaceMenu의 NIO/SKEE 파일이 런타임에 존재해야 합니다. RaceMenu를 최신 버전으로 업데이트하라는 요구는 없습니다.

## 빌드

GitHub 저장소에 이 폴더의 내용을 올리면 `.github/workflows/build.yml`이 다음 순서로 실행됩니다.

1. 정적 소스 검사 및 공백 검사
2. CommonLibSSE-NG와 SKSE Menu Framework API 확보
3. xmake 설정 검사
4. `InteractionStateTests` 빌드 및 실행
5. `HeelsFixNative.dll` 빌드
6. 최종 정적 검사 및 ESP/Papyrus 산출물 차단
7. 설치 파일을 `dist/HeelsFixNG-NoESP-SE1597/`에 구성하고 해당 디렉터리를 **Artifacts**로 업로드합니다.

각 단계가 실패하면 이후 단계와 배포 산출물 업로드를 중단합니다. GitHub Actions의 `upload-artifact`가 설치 파일을 ZIP으로 묶으므로, 이미 압축한 ZIP 파일을 다시 업로드하지 않습니다. 해당 실행의 **Artifacts**에서 받은 ZIP을 바로 모드 매니저로 설치할 수 있습니다. 로컬 빌드 스크립트는 `dist/HeelsFixNG-NoESP-SE1597.zip`을 한 번 생성합니다.

Windows에서 로컬로 빌드하려면 Git, Python 3.12+, xmake 3.1.1+ 및 Visual Studio 2026 C++ Build Tools(MSVC 14.50 이상)가 필요합니다. CommonLibSSE-NG 라이브러리와 소비자 플러그인의 MSVC 도구 집합을 맞추지 않으면 `__std_replace_copy_2` 또는 `__std_regex_transform_primary_char` 링크 오류가 발생할 수 있습니다. 저장소 루트에서 다음 명령을 실행합니다.

```powershell
.\Build-HeelsFixNG-NoESP.ps1
```

정적 검사와 xmake 구성만 검사하고 컴파일하지 않으려면 다음을 사용합니다.

```powershell
.\Build-HeelsFixNG-NoESP.ps1 -SkipBuild
```

## 설치

GitHub Actions 빌드가 성공한 뒤 산출물 ZIP을 모드 매니저로 설치합니다. 또는 ZIP 내부 `SKSE/Plugins/`의 DLL과 INI를 게임의 `Data/SKSE/Plugins/` 아래에 둡니다.

## MSVC 도구 집합 고정

- GitHub Actions는 `windows-2025`와 Visual Studio 2026을 사용하고, xmake 구성에 `--vs=2026`을 명시합니다.
- CI와 로컬 빌드 스크립트는 빌드 전에 MSVC 14.50 이상(v145)인지 검사합니다. 조건을 만족하지 않으면 C++ 컴파일/링크를 시작하지 않습니다.

## 검증 상태

- 이 작업 환경에서 `InteractionStateTests.cpp`를 Clang으로 직접 컴파일하고 실행했습니다. 회귀 테스트는 통과했습니다.
- 이 환경에는 Windows/MSVC와 xmake/CommonLib 빌드 의존성이 준비되어 있지 않아 **전체 `HeelsFixNative.dll`을 여기서 빌드하지 못했습니다**.
- 따라서 전체 플러그인의 컴파일 성공 및 Skyrim 게임 내 가구 보정 동작은 아직 검증 완료로 표시하지 않습니다. 해당 검증은 저장소의 GitHub Actions 빌드와 실제 게임 테스트로 확인해야 합니다.

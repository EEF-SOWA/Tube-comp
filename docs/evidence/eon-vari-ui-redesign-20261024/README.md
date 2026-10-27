# EON-Vari mu — 레이아웃·VU 재설계 증거 (2026-10-24)

사용자 요청에 따라 채널·유틸리티 배치, 노브 간격과 크기 계층, VU 미터
바늘 움직임을 다시 설계한 기록이다. 670 실물 사진은 형태와 질감을 판단하는
시각 참고로만 사용했고, 화면은 EON 고유 코드로 그린다.

## 1. 식별자와 결과

- macOS 15.7.9 (arm64)
- VST3 실행 파일 SHA-256: `d8fa6511ee8491a0444673304e5694bc6745a3acb277075a1dd2a5f4a08e7123`
- AU 실행 파일 SHA-256: `69fd78a4b5b4ad3aaea2ee99b45bf8d4cfd2d08aafa58946a6995acc9c4cf372`
- 빌드 산출물과 `~/Library/Audio/Plug-Ins`의 설치본은 두 포맷 모두 위
  해시와 일치한다. 전문은 `artifact-sha256.txt`에 있다.
- CTest: **15/15 통과**, 20.85초 (`ctest-final.log`)
- VU 바늘 회귀: 상승 후 peak **1.014**, 48프레임 뒤 정착 **yes**, 하강
  종료 **0.0000** (`processor-test-final.log`)

## 2. 바뀐 것

- 패널 인셋을 하나로 묶어 왼쪽·오른쪽 채널과 하단 유틸리티가 같은
  기준선을 쓴다. 채널 사이는 8 px, 유틸리티와 채널 사이는 14 px다.
- 각 채널은 미터 + 큰 노브 5개다. 미터는 폭 150–198 px, 종횡비 0.80,
  컨트롤과 18 px 간격을 두고, 노브 지름은 가용 높이의 80%와 120–220 px
  범위 안에서 반응한다. Threshold 링은 14 px 넓은 셀과 다이얼로 분리했다.
- 하단 유틸리티는 트림 노브 4개와 스위치 8개다. 스위치는 폭에 관계없이
  4×2를 유지하며, 작은 화면에서 라벨이 잘리지 않는다. BYPASS는 라벨 없이
  토글 영역 전체를 쓴다.
- 채널 사이와 유틸리티 위의 반복 가로 seam을 없애고, 채널 헤더와 콘텐츠
  인셋을 맞췄다. 980 px 폭에서도 라벨·눈금·미터가 겹치지 않는다.
- VU는 60 Hz로 갱신하고 실제 경과 시간으로 스프링을 적분한다. 상승
  stiffness 336, 하강 252, damping 26이며 약 1.4% 오버슈트 뒤 정착한다.
  정지 임계값 안에서는 정확히 목표값으로 스냅해 미세한 바늘 떨림을 없앴다.
- 바늘은 테이퍼 폴리곤과 짧은 꼬리로 바꾸고, 끝이 인쇄 아크 안쪽에서
  멈추게 했다. 피벗 캡은 바늘 위에 그린다. 표시 모드를 바꿔도 위치를
  강제로 0으로 되돌리지 않는다.
- 노브 바디 비율은 0.67, 링 다이얼은 0.64로 키웠다. 작은 노브는 눈금 7개,
  큰 노브는 11개를 써서 크기 계층이 읽히게 했다.

## 3. 화면 증거

- `editor-980x800.png` — 최소 폭. 하단 4×2 스위치와 라벨 확인.
- `editor-1180x820.png` — 기본 크기.
- `editor-1600x1120.png` — 큰 화면에서 노브와 미터 비례 확인.

최종 빌드 뒤 세 파일을 다시 렌더했고, 저장본과 바이트 단위로 일치했다.
스냅샷 SHA-256은 각각 다음과 같다.

- 980: `a99f9d091798b307483987a1e178d543c8d6878437ee3c1af5c8b187e189750d`
- 1180: `36d088b50b176b8c2f6092bb511daa8841b71066611d7f5909cd5956d6241e6a`
- 1600: `2a6fcd3133551a1792c292b64a7159ba11113c58da204b43a397e37d269aac01`

## 4. 검증 게이트

- pluginval 1.0.4, strictness 10, VST3: **SUCCESS** (`pluginval-vst3.log`).
  44.1/48/96 kHz와 블록 64/128/256/512/1024를 검사했고, 자동화는
  서브블록 32로도 검사했다. pluginval의 별도 `vst3 validator` 단계는
  validator 경로가 설정되지 않아 건너뛰었다.
- pluginval 1.0.4, strictness 10, AU: **SUCCESS** (`pluginval-au.log`).
  기존의 `Current program is -1` 경고 한 건이 남아 있다.
- auval: `auval -v aufx Eonc EEFL` → **AU VALIDATION SUCCEEDED**
  (`auval.log`).

## 5. 하지 않은 검증

- REAPER와 Ableton Live에서 새 바이너리를 열어 UI 조작·자동화·리콜을
  다시 확인하지 않았다. pluginval의 Editor 테스트와 설치 완료까지만 확인했다.
- 실제 670 하드웨어와의 A/B, 레벨 매칭 블라인드 청취가 없다. 이번 변경은
  DSP를 건드리지 않았으며, 현재 사운드는 기존 모델의 결과다.
- 따라서 이 문서는 UI와 플러그인 로드 가능성의 증거다. 음질이나 상용
  완성도를 증명하지 않는다.

## 6. 재현

    cmake --build build-ci -j 8
    ctest --test-dir build-ci --output-on-failure
    ./build-ci/eonchild_EditorSnapshot docs/evidence/eon-vari-ui-redesign-20261024/editor-980x800.png 980 800
    ./build-ci/eonchild_EditorSnapshot docs/evidence/eon-vari-ui-redesign-20261024/editor-1180x820.png 1180 820
    ./build-ci/eonchild_EditorSnapshot docs/evidence/eon-vari-ui-redesign-20261024/editor-1600x1120.png 1600 1120
    /Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
        --validate "build-ci/eonchild_artefacts/Release/VST3/EON-Vari mu DSP.vst3"
    /Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
        --validate "build-ci/eonchild_artefacts/Release/AU/EON-Vari mu DSP.component"
    auval -v aufx Eonc EEFL

# EON-Vari mu — UI 질감 작업 증거 (2026-10-24)

Fairchild 670 실물 사진을 참고해 패널 질감과 움직임을 다듬은 작업의 기록이다.
패널 그래픽은 코드로 생성한 EON 고유 자산이고, 사진은 시각 참고용으로만 썼다
(`docs/reference/fairchild/README.md`).

## 1. 식별자

- VST3 SHA-256: `33e9ea7e32026cebc7b7523bb1459edc5f6d24b35a43cae615f6efd83763ab73`
- AU SHA-256: `679994299a39a95ef5953baff3813a5ef4205643e4180f84fa712872c691a274`
- macOS 15.7.9 (24G830), arm64
- pluginval 1.0.4, strictness 10
- 스냅샷: `eonchild_EditorSnapshot` (1180×820, 980×800)

## 2. 화면

- `editor-1180x820-final.png` — 기본 창
- `editor-980x800-final.png` — 최소 폭. 하단 스위치 8개가 4열 2행으로
  재배치되어 라벨이 잘리지 않는다.

두 파일은 현재 바이너리로 다시 렌더해 바이트 단위로 일치함을 확인했다.
SHA-256 `4db19e2880105990e51423d2424f76b0781d395442c0ac7dba16a821d421ae50`,
`885821e1ec4d7291219b604bd6bd6620998c579db5d56e566bbc0bcaa4dbb713`.

이 디렉터리의 `editor-980x800.png`와 `editor-1180x820.png`는 수정 전
화면이다. 비교용으로만 남기고 최종 링크로 쓰지 않는다.

## 3. 게이트 결과

- 전체 빌드: `cmake --build build-ci -j 8` — 성공
- CTest: 15/15 통과 (20.98 s, 주석 정리 후 재실행)
- pluginval VST3: SUCCESS — 44.1/48/96 kHz × 블록 64/128/256/512/1024
- pluginval AU: SUCCESS — 같은 조합. 기존 `Current program is -1` 경고만 있음
- auval: `auval -v aufx Eonc EEFL` → AU VALIDATION SUCCEEDED

로그: `pluginval-vst3.log`, `pluginval-au.log`, `auval.log`

주석 정리 뒤 재빌드한 VST3/AU가 위 해시와 바이트 단위로 같아서, 이 검증 결과는
현재 빌드에도 그대로 해당한다.

## 4. DSP 회귀 (새 바이너리, 48 kHz / 512 샘플)

UI만 바꿨으므로 DSP가 그대로여야 한다. `eonchild_SoundBaseline` 전문은
저장하지 않았고, 다음 행을 확인했다.

- 1 kHz, −12 dBFS, wdf-4x: THD 31.87%, H2 −10.0 dBc, H3 −34.3 dBc
- 1 kHz, 0 dBFS, wdf-4x: THD 47.37%, H3 −16.2 dBc
- 비정합 프로브 7001 Hz, −3 dBFS: wdf-4x alias −38.4 dBc, fast-4x −40.2 dBc
- S3 트랜스포머 On−Off, wdf-4x: 40 Hz +0.20 dB / 1 kHz −0.60 dB /
  15 kHz −0.70 dB
- 비용: wdf 1x 2.53%, 4x 11.29%, 8x 20.85%. 트랜스포머 On에서 8x 21.54%

절대 CPU %는 머신 부하에 따라 흔들린다(2026-09-30 기록은 8x 13.1%). 같은
세션 안의 상대 비교로만 읽는다.

아울러 `eonchild_NullRender`로 결정적 렌더를 한 번 남겼다.
SHA-256 `758f4ae56e374cba43f68591bc7e17abc43b1b052892204aef0d7c76215c69b6`
(48 kHz, 512, 240000 샘플, peak 4.540, rms 2.019). 비교할 이전 렌더가 없어
널 비교가 아니라 앞으로의 DSP 변경을 대조할 지문으로만 쓴다.

## 5. 검증하지 않은 것

- 새 바이너리를 REAPER와 Ableton Live에서 다시 열어 로드·조작·청취하지 않았다.
  빌드 시 `~/Library/Audio/Plug-Ins`에 자동 설치된 상태까지만 확인했다.
- 실제 670 하드웨어와의 A/B, 레벨 매칭 블라인드 청취가 없다.
- 따라서 이 기록은 UI와 로드 가능성의 증거이며 상용 음질의 증거가 아니다.

## 6. 재현

    cmake --build build-ci -j 8
    ctest --test-dir build-ci --output-on-failure
    ./build-ci/eonchild_EditorSnapshot docs/evidence/eon-vari-ui-fairchild-20261024/editor-1180x820-final.png 1180 820
    ./build-ci/eonchild_EditorSnapshot docs/evidence/eon-vari-ui-fairchild-20261024/editor-980x800-final.png 980 800
    ./build-ci/eonchild_SoundBaseline
    ./build-ci/eonchild_NullRender /tmp/eon-null.raw
    /Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 \
        --validate "$HOME/Library/Audio/Plug-Ins/VST3/EON-Vari mu DSP.vst3"
    auval -v aufx Eonc EEFL


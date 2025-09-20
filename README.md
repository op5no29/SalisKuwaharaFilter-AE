# Salis Kuwahara Filter for Adobe After Effects 2025

Painterly, edge-preserving Kuwahara filter plugin for After Effects 2025 (Apple Silicon / cross-platform ready).

## Features
- **Painterly look** (structure tensor + sector Kuwahara, softness blend)
- **Realtime-friendly**: SmartFX (PreRender/SmartRender), ROI 尊重、半径に応じたキャッシュ
- **Controls**: Radius / Sectors / Anisotropy / Softness / Mix
- **Depth**: 8/16 bpc（32f は検証後に広告予定）

## Requirements
- macOS 14+, Apple Silicon
- After Effects 2025
- Xcode 16.x
- After Effects SDK (`AfterEffectsSDK/Examples`) ※ローカルに置くだけ。リポジトリには含めません

## Build (macOS / arm64)
```bash
# プロジェクト直下で（Paths は必要に応じて調整）
SDK="$HOME/Desktop/ae25.2_20.64bit.AfterEffectsSDK/AfterEffectsSDK/Examples"
xcodebuild -project Xcode/SalisKuwaharaFilter.xcodeproj \
  -scheme SalisKuwaharaFilter -configuration Release -arch arm64 \
  OTHER_LDFLAGS='$(inherited) -Wl,-exported_symbol,_EffectMain -Wl,-exported_symbol,_PluginDataEntryFunction2'
````

## Install to AE

```bash
PLUG="/Applications/Adobe After Effects 2025/Plug-ins/SalisKuwaharaFilter.plugin"
RSRC="$PLUG/Contents/Resources/SalisKuwaharaFilter.rsrc"
sudo rm -rf "$PLUG"
sudo cp -R ~/Library/Developer/Xcode/DerivedData/**/Build/Products/Release/SalisKuwaharaFilter.plugin "$PLUG"

# PiPL は **.rsrc にのみ** Rez（Mach-O へは書かない）
SDK="$HOME/Desktop/ae25.2_20.64bit.AfterEffectsSDK/AfterEffectsSDK/Examples"
/usr/bin/sudo /usr/bin/Rez -o "$RSRC" -useDF AEAdapter/PiPL.r -i "$SDK/Headers" -i "$SDK/Resources" -D AE_OS_MAC=1

sudo codesign -s - --deep --force "$PLUG"
```

## Verify

```bash
BIN="$PLUG/Contents/MacOS/SalisKuwaharaFilter"
nm -gjU "$BIN" | grep -E 'EffectMain|PluginDataEntryFunction2'   # エクスポート確認
/usr/bin/DeRez "$RSRC" -useDF -only PiPL | grep -E 'eVER|eGLO|eGL2|ePVR'  # 1.0 / 2.0 / 0x0320 / 0x0400
```

## Troubleshooting

### “Version mismatch: Code 1.0 / PiPL 0.2”

1. **Mach-O に PiPL を書かない**（過去に誤って書いた場合は削除）

   ```bash
   xattr -d com.apple.ResourceFork "$PLUG/Contents/MacOS/SalisKuwaharaFilter" || true
   ```
2. `.rsrc` を再Rez → 再署名
3. AE キャッシュを削除して再起動

   ```bash
   pkill -f "Adobe After Effects" 2>/dev/null || true
   pkill -f "aerendercore" 2>/dev/null || true
   rm -f "$HOME/Library/Preferences/Adobe/After Effects"/25.*/"Plugin Loading Cache"* 2>/dev/null || true
   rm -rf "$HOME/Library/Caches/Adobe/After Effects"/25.* 2>/dev/null || true
   ```
4. それでも解決しない場合は `AE_Effect_Match_Name` を一時的にユニーク化して**新規インポート**を強制

### 画が“筆致”でなく“全体ブラーに寄る”

* Softness を 0.0–0.3、Anisotropy 0.4–0.8、Sectors 6–8、Radius 8–16 を試す
* 8/16bpc の入出力スケール（8:255, 16:32768, 32f:0–1）を守る
* ROI/padding 境界の扱いを確認（SmartFX で result\_rect / max\_result\_rect を適切に設定）

## Parameters

* **Radius** (px): サンプリング半径
* **Sectors**: 方向分割（例 4/6/8）
* **Anisotropy**: 構造テンソルからの伸長比
* **Softness**: 最小分散セクタへの寄せ具合
* **Mix**: 元画像とのブレンド（%）

## Roadmap

* 32f の正式サポート広告（OutFlags2 に `PF_OutFlag2_FLOAT_COLOR_AWARE` を追加予定）
* MFR/OMP の最適化・安定化

## License

MIT (予定。必要に応じて変更)
# GRPD Agent 寮€鍙戣€呮寚鍗楋細AI Skills 涓?MCP 鏋舵瀯璇存槑

鏈枃浠惰缁嗛槓杩颁簡 GRPD 椤圭洰涓负 AI 鍔╂墜锛堝 Antigravity锛夐噺韬畾鍒剁殑 **AI Skills锛堜笓灞炴妧鑳斤級** 涓?**MCP锛圡odel Context Protocol锛屾ā鍨嬩笂涓嬫枃鍗忚锛夋湇鍔″櫒** 鐨勯€昏緫鏋舵瀯鍙婇厤缃鑼冿紝浠ヤ究寮€鍙戣€呭拰 AI 鍔╂墜鑳藉湪姝ら」鐩笂楂樻晥鍦板崗鍚屽伐浣溿€?

---

## 涓€銆?鏁翠綋璁捐鎬濇兂

涓轰簡璁?AI 鍔╂墜鍏峰楂樻按鍑嗙殑 CAE锛堣绠楁満杈呭姪宸ョ▼锛変笌 PD锛堣繎鍦哄姩鍔涘锛夊紑鍙戣兘鍔涳紝鏈」鐩噰鐢ㄤ簡 **鍙岃建椹卞姩鏋舵瀯**锛?
1. **AI Skills 鎶€鑳戒綋绯?(`.agents/skills/`)**锛氬厖褰?AI 鍔╂墜鐨勨€滀笓涓氱煡璇嗗簱涓庢祦绋嬫寚寮曗€濄€傚叾涓?`superpowers` 淇濆瓨閫氱敤娴佺▼鎶€鑳斤紝`superCAE` 淇濆瓨 GRPD/CAE 涓撻」鎶€鑳斤紝鍖呭惈鏈瀯鏁板鎺ㄥ銆佹眰瑙ｅ惊鐜畨鍏ㄣ€佺墿鐞嗗満閾捐矾瀹¤銆丱penMP 浼樺寲銆佺綉鏍奸偦鍩熸€ц兘绛変笓椤归鍩熻鑼冿紝浣垮緱 AI 鍦ㄥ垎鏋愭垨淇敼浠ｇ爜鏃惰兘閬靛惊缁熶竴鐨勯」鐩爣鍑嗐€?
2. **MCP 宸ュ叿鏈嶅姟鍣?(`.agents/mcp/`)**锛氬厖褰?AI 鍔╂墜鐨勨€滃閮ㄨ鍔ㄦ墜鑷傗€濓紝閫氳繃杩炴帴鏈湴鐜锛堝椹卞姩 Python 鑴氭湰銆丄NSYS 鏈夐檺鍏冨姣斻€丼QLite 瀹為獙搴撱€丮ATLAB 鍗曠偣绉垎姣斿绛夛級锛岃 AI 鍔╂墜鑳藉鐩存帴杩涜鑷姩缂栬瘧銆佸啋鐑熸祴璇曘€佽宸垎鏋愪笌瀹為獙鍙傛暟杩借釜銆?

```mermaid
graph TD
    A[Gemini IDE / AI Agent] -->|鍔犺浇瑙勮寖| B(AI Skills 浣撶郴)
    A -->|璋冪敤宸ュ叿| C[MCP Servers]

    subgraph AI Skills 浣撶郴 (.agents/skills/)
        B1[superpowers: 閫氱敤娴佺▼]
        B2[superCAE: 鐗╃悊鍦?鏈瀯閾捐矾瀹¤]
        B3[superCAE: 缃戞牸閭诲煙涓嶰penMP浼樺寲]
        B4[superCAE: 姹傝В涓诲惊鐜畨鍏ㄨ瘎浼癩
    end

    subgraph MCP Servers (.agents/mcp/)
        C1[ansys-server] -->|璋冪敤| D[ANSYS APDL / 鎻愬彇璇樊涓庡垏鐗囧姣擼
        C2[grpd-server] -->|缂栬瘧杩愯| E[GRPD 姹傝В鍣╙
        C3[grpd-experiment-server] -->|鏁版嵁璁板綍| F[(SQLite 绠椾緥瀹為獙搴?]
        C4[matlab-server] -->|鍗曠偣姣斿| G[MATLAB 楠岃瘉绔笌鏈瀯瀵规瘮]
    end
```

---

## 浜屻€?AI Skills 鎶€鑳戒綋绯昏鏄?

Skills 瀛樻斁浜庨」鐩牴鐩綍涓嬬殑 `.agents/skills/` 鐩綍涓€傞€氱敤娴佺▼鎶€鑳戒綅浜?`.agents/skills/superpowers/`锛孏RPD/CAE 涓撻」鎶€鑳界粺涓€浣嶄簬 `.agents/skills/superCAE/skills/`銆傛瘡涓€涓笓椤瑰瓙鐩綍閮藉寘鍚竴涓牳蹇冪殑 `SKILL.md` 鎸囧紩锛屼互鍙婇厤濂楃殑 `references/` 瑙勮寖鏂囨。鎴?`scripts/` 宸ュ叿鑴氭湰锛?

| 鎶€鑳藉悕绉?| 瀵瑰簲璺緞 | 鏍稿績鑱岃矗璇存槑 |
| :--- | :--- | :--- |
| **grpd-cae-router** | `skills/superCAE/skills/grpd-cae-router` | 鏍规嵁 CAE 褰卞搷闈㈤€夋嫨涓撻」 skill銆侀闄╁瓧娈靛拰楠岃瘉璺緞锛屾槸 Superpowers 涓?SuperCAE 鐨勮繛鎺ュ叆鍙ｃ€?|
| **cae-input-schema-validator** | `skills/superCAE/skills/cae-input-schema-validator` | 鐢ㄤ簬妫€楠屻€佽嚜鍔ㄨˉ鍏ㄣ€佹牸寮忓寲 YAML锛堝 `PD.yaml`锛夎緭鍏ュ崱锛岀‘淇濋噺绾蹭笌榛樿鍊肩鍚堝紩鎿庤姹傘€?|
| **cmake-build-doctor** | `skills/superCAE/skills/cmake-build-doctor` | 璇婃柇鍜岀淮鎶?C++17銆丱penMP銆丒igen 绛夋瀯寤洪摼璺紝闃叉 CMake 鏋勫缓鑴氭湰缂哄け婧愭枃浠躲€?|
| **constitutive-math-reviewer** | `skills/superCAE/skills/constitutive-math-reviewer` | 涓撴敞浜庢湰鏋勬洿鏂扮畻娉曪紙濡?Voigt/寮犻噺杞崲銆丣2/JC 濉戞€с€丩emaitre 鎹熶激绛夎繑杩樻槧灏勶級鐨勬暟瀛︽帹瀵间笌鏁板€肩ǔ瀹氬鏌ャ€?|
| **grpd-cae-toolkit** | `skills/superCAE/skills/grpd-cae-toolkit` | 閫氱敤 CAE 寮€鍙戣剼鎵嬫灦锛屽寘鎷悇绉嶇墿鐞嗗満銆佺姸鎬佸彉閲忓垎閰嶃€佹眰瑙ｅ櫒鍒濆鍖栧強杈撳嚭鍏变韩閾捐矾鐨勬暟鎹璁¤剼鏈€?|
| **grpd-experiment-tracker** | `skills/superCAE/skills/grpd-experiment-tracker` | 瀵瑰洖褰掓祴璇曘€佹敹鏁涙€у垎鏋愯繘琛屽巻鍙叉瘮瀵瑰拰瀹為獙杩借釜锛屽熀浜?SQLite 绠＄悊涓嶅悓鐗堟湰鎴栧弬鏁颁笅鐨勬眰瑙ｇ粨鏋溿€?|
| **grpd-smoke-tester** | `skills/superCAE/skills/grpd-smoke-tester` | 涓€閿紡鍏ㄦ祦绋嬪啋鐑熸祴璇曢獙璇佹妧鑳姐€傝皟鐢?GRPD 姹傝В銆丄NSYS 缁忓吀姹傝В骞惰緭鍑鸿瀺鍚堣宸姤鍛娿€?|
| **material-model-implementer** | `skills/superCAE/skills/material-model-implementer` | 鎸囧紩濡備綍瀹炵幇鍜屽悜宸ュ巶娉ㄥ唽鏂扮殑鏉愭枡瀛愮被锛堝 Mechanical / Thermal Material锛夛紝浠ュ強鍒嗛厤涓庢彁浜ゅ巻鍙茬姸鎬佸彉閲忋€?|
| **mesh-and-neighbor-reviewer** | `skills/superCAE/skills/mesh-and-neighbor-reviewer` | 绮掑瓙缃戞牸鎷撴墤銆佽繎閭?CSR 鍒楄〃浠ュ強楂橀鎺ヨЕ鎼滅储鐨勭┖闂寸綉鏍肩畻娉曞悎鐞嗘€т笌鎬ц兘瀹℃煡銆?|
| **numerical-test-generator** | `skills/superCAE/skills/numerical-test-generator` | 璁捐 patch test銆佸崟鐐圭Н鍒嗘媺浼哥瓑楂樺彲闈犳€х殑鏁板€兼敹鏁涗笌璇樊鍥炲綊娴嬭瘯绠椾緥銆?|
| **openmp-kernel-optimizer** | `skills/superCAE/skills/openmp-kernel-optimizer` | 璇勪及鐑惊鐜唬鐮侊紝纭繚杩炵画鍦鸿８鎸囬拡鐩存帴璁块棶鐨勯珮閫熺紦瀛樺眬閮ㄦ€т笌 OpenMP 绾跨▼瀹夊叏锛堥槻 False Sharing锛夈€?|
| **physics-field-pipeline-auditor**| `skills/superCAE/skills/physics-field-pipeline-auditor`| 妫€鏌ョ墿鐞嗗満娉ㄥ唽閾捐矾锛圥hysics Type -> Fields锛夛紝淇濋殰瀛楁琚纭垎閰嶅苟鎺ョ銆?|
| **postprocess-exporter** | `skills/superCAE/skills/postprocess-exporter` | 妫€鏌ュ悗澶勭悊 VTK / PVD 鏁版嵁鍐欏叆鐨勫瓧娈电被鍨嬶紙鏍囬噺/鍚戦噺/寮犻噺锛変笌 ParaView 鍚庡鐞嗙殑鍛藉悕瀵瑰簲瑙勮寖銆?|
| **solver-loop-safety-reviewer** | `skills/superCAE/skills/solver-loop-safety-reviewer` | 瑙勮寖鏃堕棿绉垎鍣紙濡?ADR 寮涜鲍姝ユ垨鏃犵煩闃甸殣寮忚凯浠ｏ級鍦ㄤ富寰幆涓殑鍒濆鍖栥€佽竟鐣屾柦鍔犱笌鐘舵€佹彁浜ら『搴忋€?|

---

## 涓夈€?MCP (Model Context Protocol) 鏈嶅姟閫昏緫璇存槑

MCP 鏈嶅姟绔簮鐮佷綅浜庨」鐩牴鐩綍鐨?`.agents/mcp/` 鐩綍涓紝鍚勭粍浠跺姛鑳藉涓嬶細

1. **`ansys-mcp-server` (`.agents/mcp/ansys-mcp-server`)**
   - **鍔熻兘**锛氳嚜鍔ㄧ敓鎴?APDL 瀹忓懡浠わ紝璋冨害鏈湴 ANSYS 缁忓吀鐗堝悗鍙版墽琛屽熀鍑嗚В璁＄畻锛屽苟瑙ｆ瀽 `.out`銆乣.err`銆乣.txt` 涓?`.db` 绛?ANSYS 渚х粨鏋滄枃浠躲€?2. **`grpd-mcp-server` (`.agents/mcp/grpd-mcp-server`)**
   - **鍔熻兘**锛氫负 AI 鎻愪緵涓€閿紪璇戝強杩愯 GRPD 寮曟搸鐨勫伐鍏凤紝鐩戣缁堢杈撳嚭涓庢敹鏁涙棩蹇椼€?3. **`grpd-experiment-mcp-server` (`.agents/mcp/grpd-experiment-mcp-server`)**
   - **鍔熻兘**锛氭彁渚?SQLite 鏁版嵁瀛樺偍锛岃嚜鍔ㄨВ鏋愮墿鐞嗗満鏁版嵁涓庢棩蹇楋紝鎸佷箙鍖栬褰曞悇绠椾緥鐨勬畫宸敹鏁涙洸绾裤€佺綉鏍艰妭鐐规暟銆佺墿鐞嗙敤鏃跺拰鍐呭瓨鍗犵敤銆?4. **`grpd-validation-mcp-server` (`.agents/mcp/grpd-validation-mcp-server`)**
   - **鍔熻兘**锛氳鍙?GRPD VTK 涓?ANSYS TXT锛屾墽琛岃矾寰勯噰鏍枫€佹姇褰卞榻愩€佹彃鍊艰宸绠楋紝骞惰緭鍑?Excel銆佸浘鍍忋€丣SON 涓?ZIP 楠岃瘉鎶ュ憡銆?5. **`matlab-mcp-server` (`.agents/mcp/matlab-mcp-server`)**
   - **鍔熻兘**锛氶€氳繃 MATLAB 寮曟搸杩愯鏉愭枡鐐瑰崟鐐瑰簲鍔涙洿鏂般€佺Н鍒嗗櫒瀹夊叏楠岃瘉浠ュ強鐩稿叧鐨勬湰鏋勫姣斿垎鏋愶紝渚夸簬蹇€熸牎楠屼笁缁存湰鏋勮绠楃殑鏁板€肩簿纭害銆?
---

## 鍥涖€?鍗忎綔寮€鍙戜笌閰嶇疆瑙勮寖

涓轰簡璁╁洟闃熺殑鎵€鏈夊紑鍙戣€呴『鐣呭崗浣滐紝椤圭洰寤虹珛浜嗕互涓?Git 绠＄悊瑙勫垯锛?

1. **缁濆璺緞闅旂**锛?
   - 鎵€鏈夌殑鏈湴杩愯閰嶇疆閮戒細淇濆瓨鍦?`.agents/settings.json` 涓€?
   - 璇ユ枃浠?*琚?Git 寮鸿蹇界暐**锛堝湪 `.gitignore` 涓厤缃級锛岄槻姝㈠寘鍚壒瀹氬紑鍙戣€呯鐩樼粷瀵硅矾寰勭殑閰嶇疆琚笂浼狅紝杩涜€屽鑷村叾浠栫幆澧冩棤娉曚娇鐢ㄦ垨棰戠箒鍙戠敓鍐茬獊銆?
2. **妯℃澘閰嶇疆**锛?
   - 杩滅浠撳簱璺熻釜 [.agents/settings.json.example](file:///d:/Project_C++/GRPD/.agents/settings.json.example) 浣滀负閰嶇疆妯℃澘銆?
   - 鏂扮幆澧冩媺鍙栦唬鐮佸悗锛屽簲灏?`settings.json.example` 澶嶅埗涓烘湰鍦伴潪璺熻釜鐨?`settings.json`锛屽苟灏嗗叾涓殑 `<GRPD_PROJECT_ROOT_PATH>` 鏁翠綋鏇挎崲涓烘湰鍦扮殑椤圭洰鏍硅矾寰勫嵆鍙洿鎺ヨ皟鐢ㄣ€?
3. **AI 鎶€鑳借凯浠?*锛?
   - 鍦ㄦ棩甯稿紑鍙戜腑锛屽鏋滈噸鏋勪簡绠楁硶鎴栧畾涔変簡鏂扮殑娉ㄥ唽瑙勮寖锛屽簲鍦?`.agents/skills/superCAE/skills/` 鐨勭浉鍏冲弬鑰冿紙`references/`锛変腑鏇存柊寮€鍙戝師鍒欙紙濡傚湪鏈瀯瀹℃煡涓坊鍔犳柊鐨勫眻鏈嶅嚱鏁扮Н鍒嗘爣鍑嗭級銆?
   - 淇敼鍚庣洿鎺ユ墽琛?Git 鎻愪氦鍗冲彲銆傚洜涓?`.agents/skills` 宸茬粡琚撼鍏ヤ簡鐗堟湰鎺у埗锛屼笅涓€娆?AI 鍔╂墜杞藉叆姝ら」鐩椂灏变細鑷姩閬靛惊鏈€鏂扮殑璁捐鍜屾€ц兘瑙勭害杩涜浠ｇ爜鐢熸垚涓庝慨鏀广€?

# ANSYS MCP Server

鏈湇鍔℃槸 GRPD 楠岃瘉閾句腑鐨?ANSYS 渚?MCP銆傚畠鍙礋璐ｇ敓鎴?APDL銆佽皟鐢?MAPDL銆佽В鏋?ANSYS 杈撳嚭鏂囦欢锛屽苟鎶?`.txt/.db/.out/.err` 绛夎矾寰勮繑鍥炵粰涓婂眰浠ｇ悊銆?
GRPD VTK 涓?ANSYS TXT 鐨勮宸姣斿凡缁忚縼绉诲埌 `.agents/mcp/grpd-validation-mcp-server`锛屾湰鏈嶅姟涓嶅啀鍏紑 VTK/TXT 瀵规瘮 tool銆?
## 鐩綍缁撴瀯

```text
ansys-mcp-server/
鈹溾攢鈹€ requirements.txt
鈹溾攢鈹€ README.md
鈹溾攢鈹€ server.py
鈹溾攢鈹€ config.yaml
鈹溾攢鈹€ src/
鈹?  鈹溾攢鈹€ __init__.py
鈹?  鈹溾攢鈹€ paths.py
鈹?  鈹溾攢鈹€ runner.py
鈹?  鈹溾攢鈹€ generator.py
鈹?  鈹溾攢鈹€ result_parser.py
鈹?  鈹溾攢鈹€ history.py
鈹?  鈹斺攢鈹€ service.py
鈹溾攢鈹€ templates/
鈹?  鈹斺攢鈹€ README.md
鈹斺攢鈹€ tests/
    鈹溾攢鈹€ __init__.py
    鈹溾攢鈹€ conftest.py
    鈹溾攢鈹€ test_public_tools.py
    鈹斺攢鈹€ test_standard_layout.py
```

`server.py` 鏄?FastMCP facade锛屽彧璐熻矗娉ㄥ唽鍏紑 tool 骞惰矾鐢卞埌 `src.service`銆傛牳蹇冮€昏緫蹇呴』鏀惧湪 `src/`銆?
## 鍏紑 Tools

- `run_ansys_mac(...)`
- `generate_ansys_apdl_from_yaml(...)`
- `run_ansys_yaml_case(...)`
- `get_ansys_solve_status(...)`
- `get_ansys_text_results(...)`
- `list_ansys_files(...)`

## 瀹夎

```powershell
cd D:\Project_C++\GRPD\.agents\mcp\ansys-mcp-server
python -m pip install -r requirements.txt
```

## 閰嶇疆

缂栬緫 `config.yaml`锛?
- `ansys_executable`: ANSYS MAPDL 鍙墽琛屾枃浠惰矾寰勩€?- `allowed_directories`: 鍏佽 ANSYS MCP 璁块棶鐨勫伐浣滅洰褰曠櫧鍚嶅崟銆?- `defaults`: 榛樿 CPU銆佸唴瀛樺拰瓒呮椂璁剧疆銆?
## 娴嬭瘯

```powershell
cd D:\Project_C++\GRPD
python -m pytest .agents/mcp/ansys-mcp-server/tests -q
```

杩欎簺娴嬭瘯鍙獙璇佸鍏ャ€佸叕寮€ tool 杈圭晫鍜岀洰褰曠粨鏋勶紝涓嶇湡瀹炲惎鍔?ANSYS銆?

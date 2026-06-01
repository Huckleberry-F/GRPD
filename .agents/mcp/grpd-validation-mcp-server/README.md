# GRPD Validation MCP Server

鏈湇鍔¤礋璐ｈ法姹傝В鍣ㄩ獙璇佸悗澶勭悊锛氳鍙?GRPD VTK 涓?ANSYS TXT锛屾部鎸囧畾涓夌淮閲囨牱绾胯繘琛岀偣浜戣繃婊ゃ€佹姇褰卞榻愩€佹彃鍊笺€佽宸粺璁★紝骞跺鍑?Excel銆丳NG銆丣SON 涓?ZIP 鎶ュ憡銆?
## 鑱岃矗杈圭晫

- 涓嶈繍琛?GRPD銆?- 涓嶈繍琛?ANSYS銆?- 涓嶅啓鍏ュ疄楠屽巻鍙叉暟鎹簱銆?- 瀹為獙璁板綍鐢?`grpd-experiment-mcp-server` 璐熻矗銆?
## 鐩綍缁撴瀯

```text
grpd-validation-mcp-server/
鈹溾攢鈹€ requirements.txt
鈹溾攢鈹€ README.md
鈹溾攢鈹€ server.py
鈹溾攢鈹€ src/
鈹?  鈹溾攢鈹€ __init__.py
鈹?  鈹溾攢鈹€ service.py
鈹?  鈹溾攢鈹€ comparison.py
鈹?  鈹溾攢鈹€ paths.py
鈹?  鈹斺攢鈹€ result_parser.py
鈹溾攢鈹€ templates/
鈹?  鈹斺攢鈹€ README.md
鈹斺攢鈹€ tests/
    鈹溾攢鈹€ __init__.py
    鈹溾攢鈹€ conftest.py
    鈹溾攢鈹€ test_compare_grpd_ansys.py
    鈹斺攢鈹€ test_standard_layout.py
```

`server.py` 鏄?FastMCP facade锛屽彧璐熻矗娉ㄥ唽鍏紑 tool 骞惰矾鐢卞埌 `src.service`銆倂alidation MCP 涓嶉渶瑕?`runner.py` 鎴?`generator.py`锛屽洜涓哄畠涓嶆媺璧峰閮ㄦ眰瑙ｅ櫒锛屼篃涓嶇敓鎴愭眰瑙ｈ剼鏈€?
## MCP Tool

```text
compare_grpd_vtk_with_ansys_txt(...)
```

## 娴嬭瘯

```powershell
cd D:\Project_C++\GRPD
python -m pytest .agents/mcp/grpd-validation-mcp-server/tests -q
```

娴嬭瘯浣跨敤涓存椂 mock VTK/TXT锛屼笉鐪熷疄鍚姩 GRPD 鎴?ANSYS銆?

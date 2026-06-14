# [Active Skill: grpd-smoke-tester]
# 修复 ANSYS MCP 热流边界畸变与默认工作目录污染问题

为了提升 GRPD 与 ANSYS 仿真对标的精度，并解决仿真临时文件污染 MCP 程序目录的问题，本计划包含以下两个修改：
1. **边界条件模板优化**：在 APDL 自动生成时，将 `FLUX`（热流面载荷）的施加命令从节点级的 `SF, ALL, HFLUX` 改为几何线/面级的 `SFL` / `SFA`，以根除角落节点的双向热流溢出畸变。
2. **默认工作目录重构**：若调用者未指定 `work_dir`，则默认工作目录不再在 MCP 根目录下创建递增目录，而是就近设在 `yaml_file` 所属目录的 `ansys_work` 文件夹内。

---

## 拟修改文件 (Proposed Changes)

### ansys-mcp-server

#### [MODIFY] [generator.py](file:///D:/Project_C++/GRPD/.agents/mcp/ansys-mcp-server/src/generator.py)

修改 `_append_bc` 函数中对于 `is_thermal` 且为 `FLUX` 边界条件的处理。利用 `Box` 在各维度上的跨度大小，推断出边界面的法向，从而调用几何线（2D 下的 `LSEL` + `SFL`）或几何面（3D 下的 `ASEL` + `SFA`）来精确施加热量输入，防止角落溢出。

```diff
-    if is_thermal:
-        if bc_type.upper() in {"T", "TEMP"}:
-            apdl.append(f"D, ALL, TEMP, {value}")
-        elif bc_type.upper() == "FLUX":
-            apdl.append(f"SF, ALL, HFLUX, {value}")
-    else:
-        if bc_type in {"UX", "UY"} or (bc_type == "UZ" and dim == 3):
-            apdl.append(f"D, ALL, {bc_type}, {value}")
-    apdl += ["ALLSEL, ALL", ""]
+    is_flux = is_thermal and bc_type.upper() == "FLUX"
+
+    if is_flux:
+        # 根据 Box 的几何跨度推断其边界法向，从而仅对特定的垂直/水平几何边界线或面施加热流，消除角落节点处的水平/多向溢出
+        if dim == 3 and len(box) >= 6:
+            dx_box = abs(box[1] - box[0])
+            dy_box = abs(box[3] - box[2])
+            dz_box = abs(box[5] - box[4])
+            min_dim = min(dx_box, dy_box, dz_box)
+            if min_dim == dx_box:
+                apdl += [
+                    f"ASEL, S, LOC, X, {box[0]}, {box[1]}",
+                    f"SFA, ALL, 1, HFLUX, {value}",
+                    "ASEL, ALL",
+                ]
+            elif min_dim == dy_box:
+                apdl += [
+                    f"ASEL, S, LOC, Y, {box[2]}, {box[3]}",
+                    f"SFA, ALL, 1, HFLUX, {value}",
+                    "ASEL, ALL",
+                ]
+            else:
+                apdl += [
+                    f"ASEL, S, LOC, Z, {box[4]}, {box[5]}",
+                    f"SFA, ALL, 1, HFLUX, {value}",
+                    "ASEL, ALL",
+                ]
+        else:  # 2D 问题
+            dx_box = abs(box[1] - box[0])
+            dy_box = abs(box[3] - box[2])
+            if dx_box < dy_box:
+                apdl += [
+                    f"LSEL, S, LOC, X, {box[0]}, {box[1]}",
+                    f"SFL, ALL, HFLUX, {value}",
+                    "LSEL, ALL",
+                ]
+            else:
+                apdl += [
+                    f"LSEL, S, LOC, Y, {box[2]}, {box[3]}",
+                    f"SFL, ALL, HFLUX, {value}",
+                    "LSEL, ALL",
+                ]
+    else:
+        if len(box) >= 6 and dim == 3:
+            apdl += [
+                f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
+                f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
+                f"NSEL, R, LOC, Z, {box[4]}, {box[5]}",
+            ]
+        elif len(box) >= 4:
+            apdl += [
+                f"NSEL, S, LOC, X, {box[0]}, {box[1]}",
+                f"NSEL, R, LOC, Y, {box[2]}, {box[3]}",
+            ]
+
+        if is_thermal:
+            if bc_type.upper() in {"T", "TEMP"}:
+                apdl.append(f"D, ALL, TEMP, {value}")
+        else:
+            if bc_type in {"UX", "UY"} or (bc_type == "UZ" and dim == 3):
+                apdl.append(f"D, ALL, {bc_type}, {value}")
+        apdl += ["ALLSEL, ALL", ""]
+

# 架构约定

## 依赖方向

```text
app
├── editor
├── simcore
├── components
├── fileio
└── model

editor     -> model + wxWidgets
simcore    -> model
components -> model
fileio     -> model + nlohmann-json
```

`model` 和 `simcore` 不允许依赖 wxWidgets，以便在不创建窗口的情况下测试。

## 数据流

```text
用户操作
  -> CircuitDocument
  -> 网表编译
  -> SimulationEngine
  -> SimulationSnapshot
  -> 画布刷新
```

画布不直接计算逻辑门。保存模块只保存 `CircuitDocument`，不保存临时仿真状态。

## 公共接口修改

修改 `src/model` 中的数据格式、元件接口或 `src/simcore` 中的公开接口时，应在 Pull Request 中通知其他模块负责人，并同时更新 `docs/file-format.md` 和相关测试。

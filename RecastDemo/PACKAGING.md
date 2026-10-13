# RecastDemo macOS 打包流程

以下命令均在仓库根目录执行，使用 CMake 默认的单配置构建目录 `build`。

## 首次构建

准备好 Xcode Command Line Tools 和 CMake，并将 `SDL2.framework` 放到
`RecastDemo/Bin/` 下，然后执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RecastDemo -j 4
```

如果 SDL2 framework 位于其他位置，配置时指定其父目录：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_ROOT_DIR=/path/to/framework/parent
cmake --build build --target RecastDemo -j 4
```

构建产物位于 `build/RecastDemo/RecastDemo.app`。CMake 会自动将 SDL2
framework、字体和测试资源放入 `.app`，并将示例模型复制到旁边的 `model` 文件夹。

## 修改代码后重新打包

已有配置的情况下，直接重新构建即可，无需清理构建目录：

```sh
cmake --build build --target RecastDemo -j 4
open build/RecastDemo/RecastDemo.app
```

构建成功后，`.app` 已更新，可以直接运行。发布前可使用下面的命令构建全部目标并
运行测试；配置时需启用 `RECASTNAVIGATION_TESTS`（默认启用）：

```sh
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## 生成分发 ZIP

确认构建成功后，将 `.app` 和 `model` 一起打包：

```sh
recast_package_stage="$(mktemp -d /tmp/RecastDemo-package.XXXXXX)"
ditto build/RecastDemo/RecastDemo.app "$recast_package_stage/RecastDemo.app"
ditto build/RecastDemo/model "$recast_package_stage/model"
ditto -c -k "$recast_package_stage" RecastDemo-macOS.zip
```

输出为仓库根目录下的 `RecastDemo-macOS.zip`。`ditto` 会保留 SDL2 framework
内部的符号链接。`model` 文件夹中的自定义模型也会包含在 ZIP 中。

解压后保持下面的目录关系：

```text
RecastDemo.app
model/
  dungeon.obj
  nav_test.obj
  undulating.obj
```

`bin`、`gset` 无需预先打包，程序会在首次保存时创建它们。如果需要携带已有导出数据，
可在生成 ZIP 前将对应文件夹也复制到临时目录中。

## 运行和保存位置

- 模型从 `.app` 同级的 `model` 文件夹读取，移动应用时需要一起移动。
- `model/dungeon.obj` 导出为 `bin/dungeon.bin` 和 `gset/dungeon.gset`。
- 加载 `.gset` 后，后续导出仍使用它所引用的模型名。
- 同一模型在不同构建模式下保存，会覆盖同名 `.bin`；加载时需使用对应模式。
- 只读安装目录可正常启动和加载模型，保存时需要对同级 `bin`、`gset` 文件夹有写权限。
- `imgui.ini` 保存在 `~/Library/Application Support/RecastDemo/`。

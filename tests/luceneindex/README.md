# Lucene Index 性能测试工具

一个基于 Qt6 和 Lucene++ 的全文索引性能测试工具，用于对比测试不同分词器的索引创建和搜索性能。

## 功能特性

- 支持多种分词器的索引创建与搜索测试
- 每种分词器独立管理索引，互不干扰
- 可视化界面，操作简单直观
- 显示搜索耗时，方便性能对比
- 支持常见文档格式（txt、doc、docx、pdf 等）
- 易于扩展新分词器

## 项目结构

```
luceneindex/
├── CMakeLists.txt        # 构建配置
├── analyzerfactory.h     # 分词器工厂接口
├── analyzerfactory.cpp   # 分词器工厂实现
├── indexmanager.h        # 索引管理器接口
├── indexmanager.cpp      # 索引管理器实现
├── mainwindow.h          # 主窗口接口
├── mainwindow.cpp        # 主窗口实现
└── main.cpp              # 程序入口
```

## 设计说明

### 架构设计

采用工厂模式设计，核心组件如下：

```
┌─────────────────────────────────────────────────────────┐
│                     MainWindow                           │
│  ┌─────────────────────────────────────────────────┐    │
│  │              TabWidget (每个分词器一个Tab)         │    │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐         │    │
│  │  │Standard  │ │ Chinese  │ │ Whitespace│  ...    │    │
│  │  │ Analyzer │ │ Analyzer │ │ Analyzer │         │    │
│  │  └──────────┘ └──────────┘ └──────────┘         │    │
│  └─────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   AnalyzerFactory                        │
│           分词器注册表（工厂模式核心）                     │
│  - registerAnalyzer(): 注册新分词器                       │
│  - createAnalyzer(): 创建分词器实例                       │
│  - registeredAnalyzers(): 获取所有已注册分词器             │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                    IndexManager                          │
│           索引管理器（每种分词器独立实例）                  │
│  - createIndex(): 创建索引                               │
│  - search(): 搜索索引                                    │
│  - deleteIndex(): 删除索引                               │
│  - getIndexStats(): 获取索引统计信息                      │
└─────────────────────────────────────────────────────────┘
```

### 索引存储

每种分词器的索引存储在独立目录：

```
~/.local/share/test-index/
├── standard/      # 标准分词器索引
├── chinese/       # 中文分词器索引
├── simple/        # 简单分词器索引
├── keyword/       # 关键词分词器索引
└── whitespace/    # 空格分词器索引
```

## 使用方法

### 编译

```bash
cd dde-file-manager
mkdir -p build && cd build
cmake ..
cmake --build . --target test-luceneindex-demo
```

### 运行

```bash
./build/unknown-Debug/tests/test-luceneindex-demo
```

### 操作步骤

1. **选择分词器**：在 Tab 栏选择要测试的分词器类型

2. **选择索引目录**：点击"浏览"按钮，选择要创建索引的源目录

3. **创建索引**：点击"创建索引"按钮，等待索引完成
   - 进度条显示当前进度
   - 日志区域显示详细信息

4. **搜索测试**：在搜索框输入关键词，点击"搜索"
   - 结果列表显示匹配文件
   - 显示搜索耗时

5. **重建索引**：如需重建，点击"创建索引"会提示是否覆盖

6. **删除索引**：点击"删除索引"可清除当前分词器的索引数据

## 分词器介绍

### 1. Standard Analyzer（标准分词器）

**特点：**
- 按标点符号和空白字符分词
- 自动转小写
- 支持中英文混合文本

**适用场景：**
- 通用文本索引
- 英文文档为主的内容
- 需要兼顾中英文的场景

**示例：**
```
输入: "Hello World, 你好世界"
输出: ["hello", "world", "你", "好", "世", "界"]
```

### 2. Chinese Analyzer（中文分词器）

**特点：**
- 专为中文设计的分词算法
- 智能识别中文词语边界
- 对中文文本分词效果最佳

**适用场景：**
- 中文文档索引
- 中文内容搜索为主的场景

**示例：**
```
输入: "中华人民共和国国务院"
输出: ["中华人民共和国", "国务院"]
```

### 3. Simple Analyzer（简单分词器）

**特点：**
- 仅按字母分词，非字母字符作为分隔符
- 自动转小写
- 不处理数字

**适用场景：**
- 纯英文文本
- 简单的分词需求

**示例：**
```
输入: "Hello123World"
输出: ["hello", "world"]
```

### 4. Keyword Analyzer（关键词分词器）

**特点：**
- 将整个文本作为单个 Token
- 不进行任何分词处理
- 适合精确匹配

**适用场景：**
- 精确匹配搜索
- ID、编码等字段
- 不需要模糊搜索的场景

**示例：**
```
输入: "Hello World"
输出: ["Hello World"]  # 整体作为一个词
```

### 5. Whitespace Analyzer（空格分词器）

**特点：**
- 仅按空白字符（空格、制表符、换行）分词
- 保留原始大小写
- 不处理标点符号

**适用场景：**
- 程序代码索引
- 保留大小写的场景
- 日志文件分析

**示例：**
```
输入: "Hello World,Test"
输出: ["Hello", "World,Test"]
```

## 分词器性能对比建议

| 分词器 | 中文支持 | 英文支持 | 索引速度 | 搜索精度 | 推荐场景 |
|--------|---------|---------|---------|---------|---------|
| Standard | 一般 | 优秀 | 快 | 中等 | 中英混合 |
| Chinese | 优秀 | 一般 | 中等 | 高 | 中文为主 |
| Simple | 差 | 优秀 | 最快 | 低 | 纯英文 |
| Keyword | - | - | 最快 | 精确匹配 | ID匹配 |
| Whitespace | 差 | 良好 | 快 | 低 | 代码/日志 |

## 如何扩展新分词器

### 方法一：在工厂类中注册（推荐）

编辑 `analyzerfactory.cpp`，在 `registerBuiltInAnalyzers()` 函数中添加：

```cpp
void AnalyzerFactory::registerBuiltInAnalyzers()
{
    // ... 现有分词器 ...

    // 添加自定义分词器
    registerAnalyzer(
        "my_analyzer",                          // 分词器ID（用于目录名）
        "My Custom Analyzer",                   // 显示名称
        "自定义分词器的描述说明",                  // 描述
        []() { return newLucene<MyAnalyzer>(); } // 创建函数
    );
}
```

界面会自动生成新的 Tab 页，无需修改界面代码。

### 方法二：运行时动态注册

在程序启动后动态添加：

```cpp
// 在 main.cpp 或其他初始化位置
AnalyzerFactory::instance()->registerAnalyzer(
    "custom_ngram",
    "NGram Analyzer",
    "N-Gram分词器，适合模糊搜索",
    []() { return newLucene<NGramAnalyzer>(2, 3); }
);
```

### 自定义分词器实现

如果 Lucene++ 内置分词器不满足需求，可以实现自定义分词器：

```cpp
#include <lucene++/Analyzer.h>
#include <lucene++/Tokenizer.h>

class MyAnalyzer : public Lucene::Analyzer
{
public:
    Lucene::TokenStreamPtr tokenStream(
        const Lucene::String& fieldName,
        const Lucene::ReaderPtr& reader) override
    {
        // 实现自定义分词逻辑
        return newLucene<MyTokenizer>(reader);
    }

    Lucene::TokenStreamPtr reusableTokenStream(
        const Lucene::String& fieldName,
        const Lucene::ReaderPtr& reader) override
    {
        // 可重用的分词实现
        return tokenStream(fieldName, reader);
    }
};
```

## 支持的文件格式

程序会自动提取以下格式文件的内容：

**文档格式：**
- doc, docx, pdf, rtf
- odt, ods, odp
- xls, xlsx, ppt, pptx

**文本格式：**
- txt, md
- cpp, h, c, hpp, cc, cxx
- py, java, js, ts
- json, xml, html, htm, css
- sh, bash, conf, cfg, ini
- yaml, yml

## 依赖说明

- Qt6 (Core, Widgets, Concurrent)
- Dtk6 (Core, Widget)
- Lucene++
- docparser (文档内容提取)
- DTextEncoding (编码检测)

## 注意事项

1. 首次创建索引可能需要较长时间，取决于文件数量和大小

2. 索引文件存储在 `~/.local/share/test-index/` 目录，可通过删除此目录清理所有索引

3. 搜索时支持 Lucene 查询语法：
   - 单词搜索：`hello`
   - 短语搜索：`"hello world"`
   - 通配符：`hel*`
   - 模糊搜索：`hello~`

4. 不同分词器创建的索引相互独立，可以同时存在

## 常见问题

**Q: 为什么搜索不到结果？**

A: 检查以下几点：
- 确认已创建索引
- 尝试使用分词器适合的搜索词（如中文分词器使用中文关键词）
- 检查文件是否被正确索引（查看日志输出）

**Q: 如何选择合适的分词器？**

A: 根据内容特点选择：
- 中文文档：Chinese Analyzer
- 英文文档：Standard Analyzer
- 代码文件：Whitespace Analyzer
- 精确匹配：Keyword Analyzer

**Q: 索引文件可以删除吗？**

A: 可以。索引文件存储在 `~/.local/share/test-index/`，删除后会自动重建。程序界面也提供了删除索引按钮。

## 许可证

GPL-3.0-or-later

# ElasticSearch如何进行全文检索？

主要是利用倒排索引的查询结构。他将文档中的每个单词与包含该单词的文档进行关联。通常，倒排索引由单词和包含这些单词的文档列表组成。

# es的分词器有哪些？

- standard默认分词器，对单个字符进行切分，查全率高，准确率低。
- IK分词器ik_max_word：查全率和准确率交叉，性能也高。
- IK分词器ik_smart：切分力度大，性能高但是查全率和准确率一般。
- Smart Chinese分词器：查全率和准确率性能较高。
- hanlp分词器：切分力度大，查全率和准确性一般，性能高。
- Pinyin分词器：用拼音进行查询时查全率高。

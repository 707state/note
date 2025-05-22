本文参考dl444学长的项目，对其代码进行梳理和剖析，最终将其用go重构。

1. 访问 https://my.cqu.edu.cn/authserver/casLogin?redirect\_uri=https://my.cqu.edu.cn/enroll/cas，获取到一个HTML文件并获取到两个信息：Execution是login-page-flowkey标签的内容，Crypto是login-croypto标签的内容。
2. 要对密码进行加密，加密的密钥就是Crypto。这里是使用DES加密算法（ECB模式，PKCS7填充）对密码进行加密，得到一个encryptedPassword用于后续操作。
3. 基于表单的一个单点登录，要请求的URL是 https://sso.cqu.edu.cn/login ，采用POST方式，表单数据包括：username(学号)，encryptedPassword，type（固定为UserNamePassword），execution（前面获得的Execution），_eventId（固定为submit），croypto（前面获得的Crypto）；发送出去请求之后需要处理重定向，需要跳过的重定向URL有三个。
4. 接着是OAuth的逻辑，https://my.cqu.edu.cn/authserver/oauth/authorize 使用GET方法并从重定向URL中提取授权码：使用正则表达式code=(.{6})匹配6位授权码；使用授权码获取访问令牌，用POST方法访问 https://my.cqu.edu.cn/authserver/oauth/token 请求参数包括：client_id: "enroll-prod"（客户端标识），client_secret: "app-a-1234"（客户端密钥），code: 上一步获取的授权码，redirect_uri: 与获取授权码时相同的重定向URI，grant_type: "authorization_code"（表明使用授权码获取令牌）；创建正则表达式 "access_token":"(.*?)" 用于匹配JSON响应中的访问令牌，至此SSO+OAuth完成。


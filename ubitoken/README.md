ubitoken
-----------


This ubitoken contract allows users to apply, accept, trust, untrust, issue, and manage ubi tokens on
enumivo based blockchains.


## UBI信任网络

* launch：合约选择一个ENU账户作为创世UBI成员。

* apply: 新用户可以向自己认识的UBI成员发起申请，请求介绍加入UBI信任网络。

* accept：UBI成员接受申请人的申请，介绍其加入到UBI信任网络。后续该新成员每次领取的UBI token中固定比例（如1%）自动转给介绍人。介绍人和被介绍人之间强制建立双向信任关系，并且永久存在，不能撤回。每个UBI成员强制永久信任自己。

* trust/untrust: UBI成员可以主动建立到其他UBI成员的临时单向信任连接，可以随时撤销，但需要等待30天撤销才生效。每个UBI成员最多同时信任50人（含自己）。

* claim: 每个UBI成员可以每隔24h申领一次UBI token，初次申领限定100 UBI，后续每次递减0.01 UBI。每个token会始终标记申领成员。

* send: UBI成员可以单方面主动向任意成员发送UBI token。

* swap: UBI成员可以单方面主动1:1兑换任意成员的任意UBI token，唯一条件是被动方收到的UBI token必须是由其当时信任的成员所申领的。


## UBI: web of trust

* launch: one ENU accout is selected as the genesis member of UBI web of trust.

* apply: anyone can apply to a UBI member, asking him/her to introduce you to be connected to the network.

* accept: a UBI member can accept an applicant. Some percentage of the new member's claimed UBI tokens goes to the referral. Besides, the refferal and new member are forced to trust each other permanently. Also each member trust himself/herself permanently.

* claim: every UBI member can claim form his UBI token every 24 hours. For the first time 100 UBI token is issued. Then each time the amount decreased by 0.01 UBI token. Every token is tagged with the member who claimed(issued).

* send: every UBI member can send any token to any other member.

* swap: any UBI member can 1:1 swap hist tokens with any other members without permission, only if the token's issuer is trusted by the other party.



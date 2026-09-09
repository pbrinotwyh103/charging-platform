"""生成与本项目功能一致的合成充电客服问答，不冒充真实采集数据。"""

from __future__ import annotations

import json
from pathlib import Path


SYSTEM_PROMPT = (
    "你是电动汽车充电平台的中文客服。回答要简短、准确、友好；"
    "不要编造订单、价格或用户隐私，遇到安全问题优先建议停止充电并联系现场人员。"
)

# 每项均由现有客户端、服务端和数据库业务能力整理，不包含外部个人数据。
BASE_QA = [
    ("direct_connect", "如何通过充电桩编号直连？", "在用户端首页输入桩体上的完整编号，例如 URBANEV-10010001，然后点击“编号直达”。系统会直接打开该电桩详情，电桩空闲时即可预约。"),
    ("direct_connect", "编号直达提示未找到电桩怎么办？", "请核对桩体编号中的字母、数字和连字符，确认没有空格或漏输；仍找不到时可改用附近站点查询，并向管理员报告编号。"),
    ("station_search", "怎样查找附近的充电站？", "进入用户端首页，使用模拟定位或地图定位后点击附近站点查询，系统会按当前位置展示并排序可用站点。"),
    ("station_search", "为什么附近充电站列表是空的？", "先确认客户端已连接服务端并获取定位，再扩大查询范围或刷新站点；若仍为空，请检查服务端是否使用了正确的充电站数据库。"),
    ("station_detail", "如何查看一个站点有哪些充电桩？", "在站点列表中打开目标站点，详情页会显示站内电桩编号、快慢充类型、功率和当前状态。"),
    ("pile_status", "电桩显示空闲是什么意思？", "空闲表示该电桩当前没有充电订单，通常可以直接预约；提交前仍应以服务端返回的最新状态为准。"),
    ("pile_status", "电桩显示充电中还能预约吗？", "不能。充电中的电桩已有用户使用，请选择状态为空闲的电桩或稍后刷新。"),
    ("pile_status", "电桩显示离线是什么意思？", "离线表示服务端近期没有收到设备心跳，暂时不能预约或开始充电，请选择其他在线电桩。"),
    ("pile_status", "电桩显示故障应该怎么办？", "不要继续连接或使用故障电桩。请确保人车安全、选择其他电桩，并通过告警或现场联系方式通知管理员处理。"),
    ("reservation", "怎样预约充电桩？", "打开站点详情，选择状态为空闲的电桩并点击预约。预约成功后请在有效时间内到场并开始充电。"),
    ("reservation", "预约后可以取消吗？", "可以。在尚未开始充电时进入预约或充电页面执行取消；已经开始充电的订单应使用停止充电。"),
    ("reservation", "预约失败可能是什么原因？", "常见原因包括电桩已被他人抢先预约、设备离线或故障、网络中断，以及已有未完成订单。请刷新状态后重试。"),
    ("charging", "到达充电桩后怎么开始充电？", "确认预约电桩编号与现场一致并连接车辆，然后在充电页面点击开始充电；服务端确认成功后会持续更新充电状态。"),
    ("charging", "如何主动停止充电？", "进入正在充电的订单页面点击停止充电，服务端会结束订单、计算费用并释放电桩。拔枪前请确认设备已停止输出。"),
    ("charging", "管理员可以远程停止充电吗？", "可以。出现故障、异常或用户求助时，管理员可在监控页面对活动订单执行远程停止，并记录操作结果。"),
    ("charging", "客户端退出后充电会自动停止吗？", "不会仅因界面退出就停止。充电状态由服务端维护，重新登录后可恢复活动订单；需要结束时应明确点击停止充电。"),
    ("billing", "充电费用是怎样计算的？", "系统按照订单实际充电电量和站点单价计算费用，并在停止充电后完成结算；演示价格不代表真实运营价格。"),
    ("billing", "在哪里查看本次充电费用？", "停止充电并完成结算后，可在订单详情和钱包交易流水中查看电量、费用及结算结果。"),
    ("billing", "为什么最终费用和预估费用不同？", "预估费用基于充电中的阶段数据，最终费用按服务端记录的实际电量和结算时适用单价计算，因此可能略有差异。"),
    ("wallet", "如何查看钱包余额？", "进入用户端“我的”或钱包页面即可查看当前余额；余额和流水以服务端返回的数据为准。"),
    ("wallet", "如何给钱包充值？", "进入钱包页面选择或输入充值金额并确认。课程演示使用模拟充值，不会发生真实支付。"),
    ("wallet", "余额不足还能开始充电吗？", "如果服务端的余额校验不通过，将无法开始充电。请先在钱包页面完成演示充值后再操作。"),
    ("orders", "怎样查看历史充电订单？", "进入用户中心的订单记录，可以查看已完成订单的电桩、充电电量、费用以及起止时间。"),
    ("orders", "重新登录后怎么看正在充电的订单？", "登录成功后系统会向服务端查询活动订单；若存在未完成订单，可进入充电页面继续查看状态或停止充电。"),
    ("network", "客户端提示连接失败怎么办？", "确认 charging_server 已启动、IP 和端口填写正确，并检查两台设备是否网络互通。服务端默认演示端口为 8888。"),
    ("network", "不同电脑怎样连接同一个服务端？", "让设备接入同一局域网，在客户端填写服务端所在 Ubuntu 的局域网 IP 和 8888 端口，并确保防火墙允许该端口。"),
    ("navigation", "怎样步行导航到充电站？", "打开目标站点详情并点击步行导航，地图会以当前位置和站点坐标生成路线；需要先正确配置地图 Key。"),
    ("navigation", "地图打不开应该检查什么？", "检查网络连接、腾讯地图 Key、WebServiceAPI 权限和地图页面配置；也要确认站点经纬度有效。"),
    ("safety", "充电时闻到焦味怎么办？", "立即停止充电，远离设备并提醒周围人员，不要自行拆卸；随后联系现场管理员，必要时拨打消防或应急电话。"),
    ("safety", "充电枪或线缆破损还能使用吗？", "不能。请勿触碰裸露或破损部位，停止使用该电桩并立即通知现场管理员。"),
]

QUESTION_TEMPLATES = (
    "{question}",
    "请问，{question}",
    "作为充电用户，{question}",
    "我想了解一下：{question}",
)


def build_records() -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    sequence = 1
    for intent, question, answer in BASE_QA:
        for variant, template in enumerate(QUESTION_TEMPLATES, start=1):
            records.append({
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": template.format(question=question)},
                    {"role": "assistant", "content": answer},
                ],
                "metadata": {
                    "id": f"charging-qa-{sequence:04d}",
                    "intent": intent,
                    "source": "synthetic-project-aligned",
                    "variant": variant,
                },
            })
            sequence += 1
    return records


def write_jsonl(path: Path, records: list[dict[str, object]]) -> None:
    path.write_text(
        "".join(json.dumps(item, ensure_ascii=False) + "\n" for item in records),
        encoding="utf-8",
    )


def main() -> None:
    output_dir = Path(__file__).resolve().parents[1] / "data" / "customer-service"
    output_dir.mkdir(parents=True, exist_ok=True)
    records = build_records()
    # 固定每十条取一条作为验证集，保证重复生成时划分不漂移。
    validation = [item for index, item in enumerate(records) if index % 10 == 9]
    train = [item for index, item in enumerate(records) if index % 10 != 9]
    write_jsonl(output_dir / "charging_pile_qa.jsonl", records)
    write_jsonl(output_dir / "train.jsonl", train)
    write_jsonl(output_dir / "validation.jsonl", validation)
    print(f"total={len(records)} train={len(train)} validation={len(validation)}")


if __name__ == "__main__":
    main()

"""Request authentication and tenant-scope enforcement."""

import hmac
import os

from flask import request


def is_loopback() -> bool:
    return request.remote_addr in {"127.0.0.1", "::1", None}


def authenticated() -> bool:
    if is_loopback():
        return True
    configured = os.environ.get("CHARGING_API_KEY", "")
    supplied = request.headers.get("X-Analytics-Key", "")
    return bool(configured) and hmac.compare_digest(configured.encode(), supplied.encode())


def tenant_scope() -> tuple[str, bool]:
    assigned = request.headers.get("X-Tenant-Id", "").strip()
    requested = request.args.get("tenantId", assigned).strip()
    return assigned, bool(assigned and requested and assigned == requested)

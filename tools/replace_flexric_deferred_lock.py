#!/usr/bin/env python3
"""Replace one pinned FlexRIC deferred lock with explicit checked locking."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: replace_flexric_deferred_lock.py <flexric-root>")
path = Path(sys.argv[1]) / "src/xApp/plugin_agent.c"
text = path.read_text(encoding="utf-8")
old = """    lock_guard(&p->sm_ds_mtx);
    assoc_insert(&p->sm_ds, &ran_func_id, sizeof(ran_func_id), sm);
"""
new = """    int lock_rc = pthread_mutex_lock(&p->sm_ds_mtx);
    if(lock_rc != 0) {
      fprintf(stderr,
              \"KPM_DIAG ts_us=%lld phase=service-model-registration status=lock-failed array_length=1 function_id=%u revision_id=%u service_model=%.*s return_code=%d\\n\",
              ts_us, ran_func_id, revision_id, (int)sm_name_len, sm_name, lock_rc);
      sm->free_sm(sm);
      dlclose(handle);
      return;
    }
    assoc_insert(&p->sm_ds, &ran_func_id, sizeof(ran_func_id), sm);
    const int unlock_rc = pthread_mutex_unlock(&p->sm_ds_mtx);
    fprintf(stderr,
            \"KPM_DIAG ts_us=%lld phase=service-model-registration status=%s array_length=1 function_id=%u revision_id=%u service_model=%.*s return_code=%d\\n\",
            ts_us, unlock_rc == 0 ? \"after\" : \"unlock-failed\",
            ran_func_id, revision_id, (int)sm_name_len, sm_name, unlock_rc);
"""
if text.count(old) != 1:
    raise SystemExit("pinned FlexRIC lock anchor changed; refusing transformation")
path.write_text(text.replace(old, new, 1), encoding="utf-8")

#!/usr/bin/env python3
"""Replace pinned FlexRIC defer sites used by the KPM transition."""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: replace_flexric_deferred_lock.py <flexric-root>")
root = Path(sys.argv[1])

def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"pinned FlexRIC anchor changed in {path}; refusing transformation")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")

plugin = root / "src/xApp/plugin_agent.c"
replace_once(
    plugin,
    """    lock_guard(&p->sm_ds_mtx);
    assoc_insert(&p->sm_ds, &ran_func_id, sizeof(ran_func_id), sm);
""",
    """    int lock_rc = pthread_mutex_lock(&p->sm_ds_mtx);
    if(lock_rc != 0) {
      fprintf(stderr,
              "KPM_DIAG ts_us=%lld phase=service-model-registration status=lock-failed array_length=1 function_id=%u revision_id=%u service_model=%.*s return_code=%d\\n",
              ts_us, ran_func_id, revision_id, (int)sm_name_len, sm_name, lock_rc);
      sm->free_sm(sm);
      dlclose(handle);
      return;
    }
    assoc_insert(&p->sm_ds, &ran_func_id, sizeof(ran_func_id), sm);
    const int unlock_rc = pthread_mutex_unlock(&p->sm_ds_mtx);
    fprintf(stderr,
            "KPM_DIAG ts_us=%lld phase=service-model-registration status=%s array_length=1 function_id=%u revision_id=%u service_model=%.*s return_code=%d\\n",
            ts_us, unlock_rc == 0 ? "after" : "unlock-failed",
            ran_func_id, revision_id, (int)sm_name_len, sm_name, unlock_rc);
""",
)

conf = root / "src/util/conf_file.c"
for function_name in ("get_conf_db_dir", "get_conf_db_name"):
    marker = f"char* {function_name}(fr_args_t const* args)"
    start = conf.read_text(encoding="utf-8").index(marker)
    text = conf.read_text(encoding="utf-8")
    end = text.index("\n}\n", start) + 3
    body = text[start:end]
    changed = body.replace("  defer({free(line);});\n", "")
    changed = changed.replace("  defer({fclose(fp); } );\n", "")
    changed = changed.replace(
        "  return strdup(" + ("db_dir" if function_name == "get_conf_db_dir" else "db_name") + ");\n",
        "  char* result = strdup(" + ("db_dir" if function_name == "get_conf_db_dir" else "db_name") + ");\n"
        "  free(line);\n"
        "  fclose(fp);\n"
        "  return result;\n",
    )
    if changed == body or "defer(" in changed:
        raise SystemExit(f"pinned FlexRIC defer anchor changed in {function_name}")
    conf.write_text(text[:start] + changed + text[end:], encoding="utf-8")

e42 = root / "src/xApp/e42_xapp.c"
replace_once(e42, "  defer({ free(addr); } );\n", "")
replace_once(
    e42,
    "  e2ap_init_ep_xapp(&xapp->ep, addr, port);\n",
    "  e2ap_init_ep_xapp(&xapp->ep, addr, port);\n  free(addr);\n",
)

monitor = root / "examples/xApp/c/monitor/xapp_kpm_moni.c"
replace_once(monitor, "  defer({ free_e2_node_arr_xapp(&nodes); });\n", "")
replace_once(
    monitor,
    "  free(hndl);\n\n  // Stop the xApp\n",
    "  free(hndl);\n  free_e2_node_arr_xapp(&nodes);\n\n  // Stop the xApp\n",
)

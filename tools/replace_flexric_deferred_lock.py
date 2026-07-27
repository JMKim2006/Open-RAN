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




pending = root / "src/xApp/pending_event_xapp.c"
replace_once(
    pending,
    """  lock_guard(&p->pend_mtx);
  bi_map_insert(&p->pending, &fd, sizeof(fd), ev, sizeof(*ev));
""",
    """  int lock_rc = pthread_mutex_lock(&p->pend_mtx);
  assert(lock_rc == 0);
  bi_map_insert(&p->pending, &fd, sizeof(fd), ev, sizeof(*ev));
  int unlock_rc = pthread_mutex_unlock(&p->pend_mtx);
  assert(unlock_rc == 0);
""",
)
replace_once(
    pending,
    """  lock_guard(&p->pend_mtx);

  assoc_rb_tree_t* map = &p->pending.left;

  void* it = assoc_front(map);
  void* end = assoc_end(map);
  it = find_if(map, it, end, &fd, eq_int);
  return it != end;
""",
    """  int lock_rc = pthread_mutex_lock(&p->pend_mtx);
  assert(lock_rc == 0);
  assoc_rb_tree_t* map = &p->pending.left;
  void* it = assoc_front(map);
  void* end = assoc_end(map);
  it = find_if(map, it, end, &fd, eq_int);
  const bool found = it != end;
  int unlock_rc = pthread_mutex_unlock(&p->pend_mtx);
  assert(unlock_rc == 0);
  return found;
""",
)
replace_once(
    pending,
    """  lock_guard(&p->pend_mtx);

  bmr_iter_t it = bi_map_front_right(&p->pending); 
  bmr_iter_t end = bi_map_end_right(&p->pending); 
  it = find_if_bi_map_right(&p->pending, it, end, ev, eq_ev);
  return it.it != end.it;
""",
    """  int lock_rc = pthread_mutex_lock(&p->pend_mtx);
  assert(lock_rc == 0);
  bmr_iter_t it = bi_map_front_right(&p->pending);
  bmr_iter_t end = bi_map_end_right(&p->pending);
  it = find_if_bi_map_right(&p->pending, it, end, ev, eq_ev);
  const bool found = it.it != end.it;
  int unlock_rc = pthread_mutex_unlock(&p->pend_mtx);
  assert(unlock_rc == 0);
  return found;
""",
)
replace_once(
    pending,
    """    lock_guard(&p->pend_mtx);
    size_t sz = bi_map_size(&p->pending); 
""",
    """    int lock_rc = pthread_mutex_lock(&p->pend_mtx);
    assert(lock_rc == 0);
    size_t sz = bi_map_size(&p->pending);
""",
)
replace_once(
    pending,
    """    assert(sz == bi_map_size(&p->pending) + 1 );
  }
""",
    """    assert(sz == bi_map_size(&p->pending) + 1);
    int unlock_rc = pthread_mutex_unlock(&p->pend_mtx);
    assert(unlock_rc == 0);
  }
""",
)
replace_once(
    pending,
    """    lock_guard(&p->pend_mtx);
    // It returns the void* of key2. the void* of the key1 is freed
    
    void (*free_fd)(void*) = NULL; 
    ev = bi_map_extract_left(&p->pending, &fd, sizeof(int), free_fd);
""",
    """    int lock_rc = pthread_mutex_lock(&p->pend_mtx);
    assert(lock_rc == 0);
    // It returns the void* of key2. the void* of the key1 is freed
    void (*free_fd)(void*) = NULL;
    ev = bi_map_extract_left(&p->pending, &fd, sizeof(int), free_fd);
    int unlock_rc = pthread_mutex_unlock(&p->pend_mtx);
    assert(unlock_rc == 0);
""",
)

endpoint = root / "src/lib/ep/e2ap_ep.c"
replace_once(
    endpoint,
    """  lock_guard(&((e2ap_ep_t*)ep)->mtx);

  const int rc = sctp_sendmsg(
      ep->fd, (void *)ba.buf, ba.len, (struct sockaddr *)addr, sizeof(*addr),
      sri->sinfo_ppid, sri->sinfo_flags, sri->sinfo_stream, 0, 0);
  assert(rc != 0);
  if(rc == -1){
    printf("Error sending sctp message \\n");
  }
""",
    """  const int lock_rc = pthread_mutex_lock(&((e2ap_ep_t*)ep)->mtx);
  assert(lock_rc == 0);
  const int rc = sctp_sendmsg(
      ep->fd, (void *)ba.buf, ba.len, (struct sockaddr *)addr, sizeof(*addr),
      sri->sinfo_ppid, sri->sinfo_flags, sri->sinfo_stream, 0, 0);
  const int unlock_rc = pthread_mutex_unlock(&((e2ap_ep_t*)ep)->mtx);
  assert(unlock_rc == 0);
  assert(rc != 0);
  if(rc == -1){
    printf("Error sending sctp message \\n");
  }
""",
)

queue = root / "src/util/alg_ds/ds/tsn_queue/tsn_queue.c"
replace_once(
    queue,
    """ {
  lock_guard(&q->mtx);
  pthread_cond_signal(&q->cv);
 }
""",
    """ {
  const int lock_rc = pthread_mutex_lock(&q->mtx);
  assert(lock_rc == 0);
  pthread_cond_signal(&q->cv);
  const int unlock_rc = pthread_mutex_unlock(&q->mtx);
  assert(unlock_rc == 0);
 }
""",
)
replace_once(
    queue,
    """  lock_guard(&q->mtx);

  seq_push_back(&q->r, val, sz);
  pthread_cond_signal(&q->cv);
""",
    """  const int lock_rc = pthread_mutex_lock(&q->mtx);
  assert(lock_rc == 0);
  seq_push_back(&q->r, val, sz);
  pthread_cond_signal(&q->cv);
  const int unlock_rc = pthread_mutex_unlock(&q->mtx);
  assert(unlock_rc == 0);
""",
)
replace_once(
    queue,
    """  lock_guard(&q->mtx);
  return seq_size(&q->r);
""",
    """  const int lock_rc = pthread_mutex_lock(&q->mtx);
  assert(lock_rc == 0);
  const size_t size = seq_size(&q->r);
  const int unlock_rc = pthread_mutex_unlock(&q->mtx);
  assert(unlock_rc == 0);
  return size;
""",
)

monitor = root / "examples/xApp/c/monitor/xapp_kpm_moni.c"
replace_once(
    monitor,
    "    lock_guard(&mtx);\n",
    "    const int lock_rc = pthread_mutex_lock(&mtx);\n    assert(lock_rc == 0);\n",
)
replace_once(
    monitor,
    "    counter++;\n  }\n}\n",
    "    counter++;\n    const int unlock_rc = pthread_mutex_unlock(&mtx);\n    assert(unlock_rc == 0);\n  }\n}\n",
)
replace_once(monitor, "  defer({ free_e2_node_arr_xapp(&nodes); });\n", "")
replace_once(
    monitor,
    "  free(hndl);\n\n  // Stop the xApp\n",
    "  free(hndl);\n  free_e2_node_arr_xapp(&nodes);\n\n  // Stop the xApp\n",
)

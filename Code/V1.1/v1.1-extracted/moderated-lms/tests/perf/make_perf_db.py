#!/usr/bin/env python3
"""Builds the performance database for NFR-01 and NFR-11.
800 students, 8 courses in 2 departments, 50 closed assignments per course and
one open assignment per course. Every student is enrolled in every course and
has submitted every closed assignment, so there are 800 x 8 x 50 = 320,000
submissions. Every mark has an audit entry and every approval has one too. It writes straight into SQLite using the Phase 3 schema.sql.
Usage: make_perf_db.py SCHEMA_SQL OUT_DB"""
import hashlib, os, random, sqlite3, sys, time

SCHEMA, OUT = sys.argv[1], sys.argv[2]
STUDENTS, COURSES, CLOSED = 800, 8, 50
random.seed(2026)

def hash_pw(pw):
    salt = os.urandom(16)
    key = hashlib.pbkdf2_hmac("sha256", pw.encode(), salt, 100000, 32)
    return "pbkdf2$100000$%s$%s" % (salt.hex(), key.hex())

def ts(t):
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(t))

if os.path.exists(OUT):
    os.remove(OUT)
con = sqlite3.connect(OUT)
con.executescript(open(SCHEMA).read())
con.execute("PRAGMA foreign_keys = ON")
now = int(time.time())
day = 86400

# one hash per password. The 800 students share one salt, which is fine for timing.
h_admin, h_hod, h_lec, h_stu = (hash_pw(p) for p in ("Admin#2026", "Hod#2026pw", "Lect#2026pw", "Stud#2026pw"))

users = [(1, "admin", h_admin, "Administrator", "ADMIN")]
users += [(2, "hod1", h_hod, "HoD One", "HOD"), (3, "hod2", h_hod, "HoD Two", "HOD")]
users += [(3 + c, "lect%d" % c, h_lec, "Lecturer %d" % c, "LECTURER") for c in range(1, COURSES + 1)]
first_student = 3 + COURSES + 1            # id 12
users += [(first_student + i, "stu%d" % (i + 1), h_stu, "Student %d" % (i + 1), "STUDENT") for i in range(STUDENTS)]
con.executemany("INSERT INTO app_user(user_id,username,password_hash,full_name,role) VALUES (?,?,?,?,?)", users)

con.executemany("INSERT INTO department(dept_id,name,hod_id) VALUES (?,?,?)", [(1, "Sciences", 2), (2, "Humanities", 3)])
con.executemany("INSERT INTO course(course_id,dept_id,code,name) VALUES (?,?,?,?)",
                [(c, 1 if c <= 4 else 2, "C%03d" % c, "Course %d" % c) for c in range(1, COURSES + 1)])
con.executemany("INSERT INTO course_lecturer VALUES (?,?)", [(c, 3 + c) for c in range(1, COURSES + 1)])
con.executemany("INSERT INTO enrolment VALUES (?,?,?)",
                [(first_student + i, c, ts(now - 400 * day)) for i in range(STUDENTS) for c in range(1, COURSES + 1)])

# assignments: ids 1..400 closed (course c, number k), then 401..408 open
aid = {}
rows = []
n = 0
for c in range(1, COURSES + 1):
    for k in range(1, CLOSED + 1):
        n += 1
        aid[(c, k)] = n
        rows.append((n, c, 3 + c, "Assignment %d" % k, "Generated", ts(now - (CLOSED + 1 - k) * 7 * day), 20))
for c in range(1, COURSES + 1):
    n += 1
    aid[(c, 0)] = n
    rows.append((n, c, 3 + c, "Open assignment", "Generated", ts(now + 14 * day), 20))
con.executemany("INSERT INTO assignment(assignment_id,course_id,created_by,title,description,deadline,max_mark) VALUES (?,?,?,?,?,?,?)", rows)

# status of the closed assignments: 1-46 released, 47 approved, 48 marked, 49-50 unmarked
def status_of(k):
    return "RELEASED" if k <= 46 else "MODERATED" if k == 47 else "MARKED" if k == 48 else None

text = "Generated work for the performance test. " * 5
sid = 0
sub_rows, mark_rows = [], []
for c in range(1, COURSES + 1):
    hod = 2 if c <= 4 else 3
    for k in range(1, CLOSED + 1):
        deadline = now - (CLOSED + 1 - k) * 7 * day
        for i in range(STUDENTS):
            sid += 1
            late = random.random() < 0.03          # 3 percent are one day late
            when = deadline + (day // 2 if late else -random.randint(3600, 3 * day))
            sub_rows.append((sid, aid[(c, k)], first_student + i, ts(when), text))
            st = status_of(k)
            if st:
                raw = float(random.randint(6, 20))
                days = 1 if late else 0
                fin = max(0.0, raw - 2.0 * days)      # 10 percent of 20 per day
                mark_rows.append((sid, raw, days, fin, "Generated feedback", st, 3 + c, ts(when + day),
                                  hod if st in ("MODERATED", "RELEASED") else None,
                                  ts(when + 2 * day) if st == "RELEASED" else None))
con.executemany("INSERT INTO submission(submission_id,assignment_id,student_id,submitted_at,content) VALUES (?,?,?,?,?)", sub_rows)
con.executemany("INSERT INTO mark VALUES (?,?,?,?,?,?,?,?,?,?)", mark_rows)
# the audit log of a real school would hold one entry per mark and one per approval
audit = []
for (sub, raw, days, fin, fb, st, by, at, mod, rel) in mark_rows:
    audit.append((by, at, "MARK", sub, None, "raw %.1f" % raw, ""))
    if mod:
        audit.append((mod, at, "APPROVE", sub, "MARKED", st, ""))
con.executemany("INSERT INTO audit_log(actor_id,at,action,submission_id,old_value,new_value,reason) VALUES (?,?,?,?,?,?,?)", audit)
con.commit()
for t in ("app_user", "course", "enrolment", "assignment", "submission", "mark", "audit_log"):
    print(t, con.execute("SELECT count(*) FROM %s" % t).fetchone()[0])
print("integrity:", con.execute("PRAGMA integrity_check").fetchone()[0])
print("foreign key problems:", len(con.execute("PRAGMA foreign_key_check").fetchall()))
con.close()
print("size MB: %.1f" % (os.path.getsize(OUT) / 1e6))

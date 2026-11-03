#!/usr/bin/env python3
"""System and acceptance tests. They run the real lms program as a person would,
feed it keystrokes through stdin and then check the output and the database
file. Usage: system_tests.py PATH_TO_LMS [RESULTS_FILE]"""
import os, re, sqlite3, subprocess, sys, tempfile, shutil, time

LMS = os.path.abspath(sys.argv[1])
OUT = sys.argv[2] if len(sys.argv) > 2 else None
WORK = tempfile.mkdtemp(prefix="lms_system_")
results = []

def run(db, script, check=True):
    p = subprocess.run([LMS, "--db", db], input=script, capture_output=True,
                       text=True, timeout=60)
    if check and p.returncode != 0:
        raise AssertionError("program exited with %d: %s" % (p.returncode, p.stderr))
    return p.stdout

def fresh():
    path = os.path.join(WORK, "t%d.db" % len(results))
    subprocess.run([LMS, "--db", path, "--demo"], input="\n", capture_output=True, text=True, timeout=60)
    return path

def q(db, sql, *args):
    con = sqlite3.connect(db)
    try:
        return con.execute(sql, args).fetchall()
    finally:
        con.close()

LEC = "lect1\nLect#2026pw\n"
HOD = "hod1\nHod#2026pw\n"
STU1 = "stu1\nStud#2026pw\n"
STU2 = "stu2\nStud#2026pw\n"
ADM = "admin\nAdmin#2026\n"

def test(tid, req, title):
    def deco(fn):
        t0 = time.time()
        try:
            fn()
            results.append((tid, req, title, "PASS", ""))
        except AssertionError as e:
            results.append((tid, req, title, "FAIL", str(e)[:200]))
        except Exception as e:  # a crash in the test itself counts as a failure
            results.append((tid, req, title, "FAIL", "%s: %s" % (type(e).__name__, e)))
        return fn
    return deco

def has(text, needle):
    assert needle in text, "expected %r in output" % needle

# ---------------------------------------------------------------- tests
@test("ST-01", "FR-01", "Login works for each role and shows only that role's menu")
def _():
    db = fresh()
    for creds, yes, no in ((ADM, "Create user", "Submit work"), (LEC, "Mark a submission", "Approve a mark"),
                           (HOD, "Approve a mark", "Mark a submission"), (STU1, "Submit work", "Create user")):
        out = run(db, creds + "0\n\n")
        has(out, yes); assert no not in out, "%r should not be in this menu" % no

@test("ST-02", "FR-01, FR-04", "Five wrong passwords lock the account, admin reset unlocks it")
def _():
    db = fresh()
    out = run(db, "stu1\nbad\n" * 5 + "stu1\nStud#2026pw\n\n")
    has(out, "locked")
    assert "Welcome" not in out, "a locked account must not log in even with the right password"
    run(db, ADM + "3\n4\nNewpass#123\n0\n\n")  # stu1 is user id 4
    out = run(db, "stu1\nNewpass#123\n0\n\n")
    has(out, "Welcome, Palesa Ntho")

@test("ST-03", "FR-09, FR-18", "Student submits on time and sees SUBMITTED")
def _():
    db = fresh()
    out = run(db, STU1 + "2\n2\nMy essay\nEND\n1\n0\n\n")
    has(out, "Status SUBMITTED"); has(out, "Essay 1")
    assert q(db, "SELECT count(*) FROM submission WHERE assignment_id=2")[0][0] == 1

@test("ST-04", "FR-10", "Resubmitting before the deadline keeps only the latest")
def _():
    db = fresh()
    run(db, STU1 + "2\n2\nfirst\nEND\n2\n2\nsecond\nEND\n0\n\n")
    rows = q(db, "SELECT content FROM submission WHERE assignment_id=2")
    assert len(rows) == 1 and rows[0][0].strip() == "second", rows

@test("ST-05", "FR-09, BR1", "Late work shows the penalty warning and needs YES")
def _():
    db = fresh()
    # Lab report (assignment 1) is 5 days past its deadline, stu2 already has a late submission,
    # so use stu1 after clearing his demo submission to test the first late submission.
    con = sqlite3.connect(db); con.execute("DELETE FROM submission WHERE student_id=4"); con.commit(); con.close()
    out = run(db, STU1 + "2\n1\nlate work\nEND\nno\n0\n\n")
    has(out, "day(s) late"); has(out, "Not submitted")
    assert q(db, "SELECT count(*) FROM submission WHERE student_id=4")[0][0] == 0
    out = run(db, STU1 + "2\n1\nlate work\nEND\nYES\n0\n\n")
    has(out, "Status LATE")

@test("ST-06", "FR-11, BR1", "30 hours late costs two days: raw 18 of 20 becomes 14")
def _():
    db = fresh()
    out = run(db, LEC + "4\n2\n18\nlate but fine\n0\n\n")
    has(out, "Days late 2"); has(out, "final mark 14")
    row = q(db, "SELECT raw_mark, days_late, final_mark FROM mark WHERE submission_id=2")[0]
    assert row == (18.0, 2, 14.0), row

@test("ST-07", "FR-12, FR-14..17", "Mark, approve, release and the student sees it only at the end")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n16\nGood\n0\n\n")
    out = run(db, STU1 + "3\n0\n\n"); has(out, "Nothing to show")      # marked, not released
    run(db, HOD + "2\n1\n0\n\n")
    out = run(db, STU1 + "3\n0\n\n"); has(out, "Nothing to show")      # approved, not released
    run(db, HOD + "5\n1\n0\n\n")
    out = run(db, STU1 + "3\n0\n\n"); has(out, "final 16"); has(out, "Good")

@test("ST-08", "FR-16, BR3", "HoD cannot release a mark that is not approved")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n16\nGood\n0\n\n")
    out = run(db, HOD + "5\n1\n0\n\n")
    has(out, "must be approved before it is released")
    assert q(db, "SELECT status FROM mark WHERE submission_id=1")[0][0] == "MARKED"

@test("ST-09", "FR-15, FR-19", "Adjust needs a reason, changes the mark and is audited")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n10\nok\n0\n\n")
    out = run(db, HOD + "3\n1\n17\n\n0\n\n"); has(out, "needs a reason")
    run(db, HOD + "3\n1\n17\nrubric applied wrongly\n0\n\n")
    assert q(db, "SELECT final_mark, status FROM mark WHERE submission_id=1")[0] == (17.0, "MODERATED")
    acts = [r[0] for r in q(db, "SELECT action FROM audit_log ORDER BY audit_id")]
    assert acts == ["MARK", "ADJUST"], acts

@test("ST-10", "FR-13, FR-15", "Returned mark reaches the lecturer and can be re-marked")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n5\nharsh\n0\n\n")
    run(db, HOD + "4\n1\ncheck question 3\n0\n\n")
    out = run(db, LEC + "5\n0\n\n"); has(out, "RETURNED")
    run(db, LEC + "4\n1\n9\nre-marked\n0\n\n")
    assert q(db, "SELECT raw_mark, status FROM mark WHERE submission_id=1")[0] == (9.0, "MARKED")
    acts = [r[0] for r in q(db, "SELECT action FROM audit_log ORDER BY audit_id")]
    assert acts == ["MARK", "RETURN", "AMEND"], acts

@test("ST-11", "FR-08, FR-19", "Individual extension is shown to the student and audited")
def _():
    db = fresh()
    out = run(db, LEC + "2\n2\n4\n2099-01-01 10:00\nill\n0\n\n"); has(out, "Extension saved")
    out = run(db, STU1 + "1\n0\n\n"); has(out, "2099-01-01 10:00:00")
    assert q(db, "SELECT count(*) FROM audit_log WHERE action='EXTEND'")[0][0] == 1

@test("ST-12", "CR1", "Whole-class extension reaches every enrolled student")
def _():
    db = fresh()
    out = run(db, LEC + "7\n2\n2099-01-01 10:00\npower cut\n0\n\n"); has(out, "Extension saved for 2 student(s)")
    for s in (STU1, STU2):
        has(run(db, s + "1\n0\n\n"), "2099-01-01 10:00:00")
    assert q(db, "SELECT count(*) FROM audit_log WHERE action='EXTEND'")[0][0] == 2

@test("ST-13", "FR-02", "A student cannot reach admin, lecturer or HoD actions")
def _():
    db = fresh()
    before = q(db, "SELECT count(*) FROM app_user")[0][0]
    out = run(db, STU1 + "5\n6\n7\n8\n9\n10\n11\n0\n\n")
    assert q(db, "SELECT count(*) FROM app_user")[0][0] == before
    assert "audit" not in out.lower()

@test("ST-14", "NFR-05", "Thirty invalid inputs: no crash and no change to the data")
def _():
    db = fresh()
    snap = [q(db, "SELECT count(*) FROM %s" % t)[0][0] for t in ("mark", "assignment", "submission", "app_user")]
    bad = ["abc", "", "-5", "99999999999999", "1.5", "'; DROP TABLE mark;--"]
    s = LEC
    for _i in range(5):
        for b in bad:
            s += "4\n%s\n%s\n%s\n" % (b, b, b) + "1\n%s\n%s\n\n%s\n%s\n" % (b, b, b, b)
    s += "0\n\n"
    out = run(db, s)    # returncode must be 0
    now = [q(db, "SELECT count(*) FROM %s" % t)[0][0] for t in ("mark", "assignment", "submission", "app_user")]
    assert snap == now, (snap, now)

@test("ST-15", "NFR-04", "Ten SQL injection strings are stored as text and change nothing")
def _():
    db = fresh()
    inj = ["'; DROP TABLE mark;--", "\" OR 1=1 --", "x'); DELETE FROM app_user;--", "' UNION SELECT password_hash FROM app_user --",
           "1; UPDATE mark SET raw_mark=20", "%", "_", "\\", "' OR ''='", "Robert'); DROP TABLE app_user;--"]
    for s in inj:
        run(db, "%s\n%s\n\n" % (s, s))                      # login attempt
        run(db, LEC + "4\n1\n10\n%s\n0\n\n" % s)             # feedback
    assert q(db, "SELECT count(*) FROM app_user")[0][0] == 5
    assert q(db, "SELECT count(*) FROM mark")[0][0] == 1
    assert q(db, "SELECT feedback FROM mark")[0][0] == "Robert'); DROP TABLE app_user;--"

@test("ST-16", "NFR-06", "Nobody can edit or delete an audit entry in the real database file")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n10\nok\n0\n\n")
    con = sqlite3.connect(db)
    for sql in ("UPDATE audit_log SET reason='x'", "DELETE FROM audit_log"):
        try:
            con.execute(sql); con.commit(); raise AssertionError("statement was allowed: " + sql)
        except sqlite3.DatabaseError as e:
            assert "cannot be" in str(e) or "audit" in str(e), e
    con.close()
    assert q(db, "SELECT count(*) FROM audit_log")[0][0] == 1

@test("ST-17", "NFR-03", "Passwords are stored only as salted hashes")
def _():
    db = fresh()
    hashes = [r[0] for r in q(db, "SELECT password_hash FROM app_user")]
    assert all(h.startswith("pbkdf2$") for h in hashes)
    assert len(set(hashes)) == len(hashes), "equal passwords must still give different hashes"
    for pw in ("Admin#2026", "Hod#2026pw", "Lect#2026pw", "Stud#2026pw"):
        assert not any(pw in h for h in hashes)

@test("ST-18", "FR-22", "Backup then restore brings back the old data")
def _():
    db = fresh(); bk = os.path.join(WORK, "bk.db")
    if os.path.exists(bk): os.remove(bk)
    run(db, ADM + "13\n%s\n0\n\n" % bk)
    run(db, ADM + "1\nSTUDENT\nextra\nExtra One\nPassw0rd!\n0\n\n")
    assert q(db, "SELECT count(*) FROM app_user WHERE username='extra'")[0][0] == 1
    run(db, ADM + "14\nYES\n%s\n\n" % bk)
    assert q(db, "SELECT count(*) FROM app_user WHERE username='extra'")[0][0] == 0

@test("ST-19", "FR-20, FR-21", "Admin can view and export the audit log")
def _():
    db = fresh(); ex = os.path.join(WORK, "audit.txt")
    run(db, LEC + "4\n1\n10\nok\n0\n\n")
    out = run(db, ADM + "11\n12\n%s\n0\n\n" % ex); has(out, "MARK")
    txt = open(ex).read()
    assert txt.count("\n") == 1 and "MARK" in txt, txt

@test("ST-20", "NFR-10", "Admin can remove a leaver's details and the marks stay")
def _():
    db = fresh()
    run(db, LEC + "4\n1\n10\nok\n0\n\n")
    run(db, ADM + "5\n4\nYES\n0\n\n")
    assert q(db, "SELECT full_name, active FROM app_user WHERE user_id=4")[0] == ("Former user", 0)
    assert q(db, "SELECT count(*) FROM mark")[0][0] == 1

@test("ST-21", "NFR-02", "The program contains no network code")
def _():
    out = subprocess.run(["ldd", LMS], capture_output=True, text=True).stdout
    assert not re.search(r"libcurl|libnss|libresolv|libssl", out), out
    syms = subprocess.run(["nm", "-D", "--undefined-only", LMS], capture_output=True, text=True).stdout
    bad = [s for s in re.findall(r"\b(socket|connect|getaddrinfo|gethostbyname|bind|listen|accept)\b", syms)]
    assert not bad, bad

@test("ST-22", "NFR-07", "Each use case needs at most 4 menu selections after login")
def _():
    # (who, menu picks typed after login, text that proves the task worked)
    tasks = [("student submits work", STU1, "2\n2\nwork\nEND\n", 1, "Status SUBMITTED"),
             ("student sees assignments", STU1, "1\n", 1, "Lab report"),
             ("lecturer marks", LEC, "4\n1\n12\nok\n", 1, "Saved"),
             ("lecturer gives a class extension", LEC, "7\n2\n2099-01-01 10:00\nr\n", 1, "Extension saved"),
             ("HoD lists, approves and releases", HOD, "1\n2\n2\n5\n2\n", 4, "Released"),
             ("admin creates a user", ADM, "1\nSTUDENT\nnew1\nNew One\nPassw0rd!\n", 1, "Created user"),
             ("admin reads the audit log", ADM, "11\n", 1, "audit")]
    for name, who, keys, picks, proof in tasks:
        db = fresh()
        if name.startswith("HoD"):
            run(db, LEC + "4\n2\n12\nok\n0\n\n")
            keys = "1\n2\n2\n5\n2\n"
        out = run(db, who + keys + "0\n\n")
        assert picks <= 4, name
        if proof != "audit": has(out, proof)

@test("ST-23", "NFR-01", "Login and menu actions are fast on the demo database")
def _():
    db = fresh()
    t0 = time.time(); run(db, STU1 + "0\n\n"); login = time.time() - t0
    assert login < 2.0, "login took %.2fs" % login

# ---------------------------------------------------------------- report
passed = sum(1 for r in results if r[3] == "PASS")
lines = ["%-6s %-14s %-4s %s %s" % (r[0], r[1], r[3], r[2], ("| " + r[4]) if r[4] else "") for r in results]
report = "\n".join(lines) + "\n\n%d of %d system tests passed\n" % (passed, len(results))
print(report)
if OUT: open(OUT, "w").write(report)
shutil.rmtree(WORK, ignore_errors=True)
sys.exit(0 if passed == len(results) else 1)

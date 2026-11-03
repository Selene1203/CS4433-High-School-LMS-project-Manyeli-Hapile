#!/usr/bin/env python3
"""Times the real lms program on the generated database (NFR-01, NFR-11).
Each run starts a new lms process, logs in, does each action once and logs out.
An action is timed from the moment its last input line is written until the
next menu prompt ("Choice: ") appears. Typing time is not counted.
Usage: run_perf.py PATH_TO_LMS PERF_DB [RUNS] [RESULTS_JSON]
The database is changed by the run (marks, approvals, new users), so keep a
pristine copy and regenerate or copy it again before repeating."""
import json, os, re, select, statistics, subprocess, sys, time

LMS, DB = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
RUNS = int(sys.argv[3]) if len(sys.argv) > 3 else 20
OUT = sys.argv[4] if len(sys.argv) > 4 else None
FIRST_STUDENT, PER_ASSIGN = 12, 800

class Session:
    def __init__(self):
        t0 = time.perf_counter()
        self.p = subprocess.Popen([LMS, "--db", DB], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, bufsize=0)
        self.buf = b""
        self.wait("Username (blank to exit): ")
        self.startup = time.perf_counter() - t0
    def wait(self, prompt, limit=120):
        end = prompt.encode()
        t_end = time.time() + limit
        while not self.buf.endswith(end):
            r, _, _ = select.select([self.p.stdout], [], [], 1)
            if r:
                chunk = os.read(self.p.stdout.fileno(), 1 << 20)
                if not chunk:
                    raise RuntimeError("program ended: " + self.p.stderr.read().decode())
                self.buf += chunk
            elif time.time() > t_end:
                raise RuntimeError("timeout waiting for " + prompt)
        text, self.buf = self.buf.decode(errors="replace"), b""
        return text
    def timed(self, lines, prompt="Choice: "):
        t0 = time.perf_counter()
        self.p.stdin.write(("\n".join(lines) + "\n").encode())
        text = self.wait(prompt)
        return time.perf_counter() - t0, text
    def close(self):
        try:
            self.p.stdin.write(b"0\n\n"); self.p.stdin.close()
        except Exception:
            pass
        self.p.wait(timeout=30)

times, notes = {}, {}
def rec(name, secs, text=None):
    times.setdefault(name, []).append(secs)
    if text is not None:
        notes[name] = text.count("\n")

def role_run(user, pw, i, actions):
    s = Session()
    rec("Start program and open database", s.startup)
    secs, text = s.timed([user, pw])
    assert "Welcome" in text, text[:200]
    rec("Login (%s)" % actions["role"], secs)
    for name, fn in actions["list"]:
        lines, check = fn(i)
        secs, text = s.timed(lines)
        if check and check not in text:
            raise RuntimeError("%s run %d: expected %r, got %r" % (name, i, check, text[-300:]))
        rec(name, secs, text)
    s.close()

def sid(course, k, idx):          # submission id from course, assignment number, student index
    return (course - 1) * 50 * PER_ASSIGN + (k - 1) * PER_ASSIGN + idx + 1

student = {"role": "student", "list": [
    ("Student: list my assignments", lambda i: (["1"], "MARK RELEASED")),
    ("Student: view my released marks", lambda i: (["3"], "RELEASED")),
    ("Student: submit work", lambda i: (["2", "401", "Perf test work", "END"], "Submitted")),
]}
lecturer = {"role": "lecturer", "list": [
    ("Lecturer: list work waiting to be marked", lambda i: (["3"], "Submission")),
    ("Lecturer: mark a submission", lambda i: (["4", str(sid(1, 49, i)), "15", "Good work"], "Saved")),
    ("Lecturer: create assignment", lambda i: (["1", "1", "Perf %d" % i, "Generated", "2027-01-15 17:00", "20"], "Created")),
    ("Lecturer: extension for one student", lambda i: (["2", "401", str(FIRST_STUDENT + 100 + i), "2027-02-01 17:00", "Perf test"], "Extension saved")),
]}
lecturer2 = {"role": "lecturer", "list": [
    ("Lecturer: extension for the whole class (800 students)",
     lambda i: (["7", "402", "2027-01-%02d 17:00" % (i + 1), "Perf test"], "800 student")),
]}
hod = {"role": "HoD", "list": [
    ("HoD: list marks waiting for moderation", lambda i: (["1"], "Submission")),
    ("HoD: approve a mark", lambda i: (["2", str(sid(1, 48, i))], "Approved")),
    ("HoD: release a mark", lambda i: (["5", str(sid(1, 47, i))], "Released")),
]}
new_ids = {}
def create_user(i):
    return ["1", "STUDENT", "perfuser%d" % i, "Perf User %d" % i, "Test#2026pw"], "Created user"
admin = {"role": "admin", "list": [
    ("Admin: create user", create_user),
    ("Admin: list users", lambda i: (["2"], "stu800")),
    ("Admin: enrol student", lambda i: (["10", str(FIRST_STUDENT + 800 + i), "1"], "Enrolled")),
    ("Admin: back up database (158 MB)", lambda i: (["13", os.path.join(os.path.dirname(DB), "perf_backup.db")], "Backup written")),
]}
admin_audit = {"role": "admin (audit)", "list": [
    ("Admin: view audit log (about 608,000 entries)", lambda i: (["11"], "APPROVE")),
]}

t_start = time.time()
for i in range(RUNS):
    role_run("stu%d" % (i + 1), "Stud#2026pw", i, student)
    role_run("lect1", "Lect#2026pw", i, lecturer)
    role_run("lect2", "Lect#2026pw", i, lecturer2)
    role_run("hod1", "Hod#2026pw", i, hod)
    # the new users are created in the order of the runs, so the ids are 812, 813, ...
    role_run("admin", "Admin#2026", i, admin)
    role_run("admin", "Admin#2026", i, admin_audit)
    print("run %d done" % (i + 1), file=sys.stderr)

def p95(v):
    v = sorted(v); return v[min(len(v) - 1, int(round(0.95 * (len(v) - 1))))]
summary = {}
for name, v in times.items():
    summary[name] = dict(n=len(v), min=min(v), median=statistics.median(v), mean=statistics.mean(v),
                         p95=p95(v), max=max(v), output_lines=notes.get(name), raw=v)
rows = sorted(summary.items())
print("%-62s %3s %7s %7s %7s %7s %7s %8s" % ("Action", "n", "min", "median", "mean", "p95", "max", "lines"))
for name, r in rows:
    print("%-62s %3d %7.3f %7.3f %7.3f %7.3f %7.3f %8s" % (name, r["n"], r["min"], r["median"], r["mean"], r["p95"], r["max"], r["output_lines"] or ""))
if OUT:
    json.dump(summary, open(OUT, "w"), indent=1)

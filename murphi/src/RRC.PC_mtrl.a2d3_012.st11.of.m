/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


const
    VAL_COUNT: 2;
    ADR_COUNT: 4;

    U_NET_MAX: 8;

    CPU_COUNT: 2;
    CACHE_L1_COUNT: 2;
    DIR_COUNT: 3;
    INSTR_COUNT: 4;
    -- ADDR_PER_DIR: ADDR_COUNT / DIR_COUNT;

    -- below are for RRC
    TS_MAX: 1;
    CNT_MAX: 1;

type
    Address: 0..ADR_COUNT-1;
    ClValue: 0..VAL_COUNT-1;
    InstrCnt: 0..INSTR_COUNT;
    DirCnt: 0..DIR_COUNT;
    OBJSET_CPU: enum{ CPU0, CPU1 };
    OBJSET_cacheL1: scalarset(CACHE_L1_COUNT);
    OBJSET_dir: enum{ DIR0, DIR1, DIR2 };
    Machines: union{OBJSET_CPU, OBJSET_cacheL1, OBJSET_dir};

    -- below are for RRC
    RRCTimestamp: 0..TS_MAX;
    RRCCounter: 0..CNT_MAX;
    T_uncommitedTS: array[RRCTimestamp] of boolean;

    AccType: enum {
        load,
        store,
        nt_store,
        self_inv,
        barrier,
        none
    };

    AccCst: enum {
        RLX,
        ACQ,
        REL,
        SEQ
    };

    L1Perm: enum {
        L1_M_evict_x_I,
        L1_M_evict,
        L1_M,
        L1_I_store,
        L1_I_load,
        L1_I
    };

    DirPerm: enum {
        Dir_M,
        Dir_I
    };

    MessageType: enum {
        CPU_Load,
        CPU_Store,
        CPU_Ntstore,
        CPU_Selfinv,
        CPU_Barrier,
        CPU_RRC_Reclaim,
        CPU_RRC_Reclaim_Resp,
        CPU_Barrier_Resp,
        CPU_Load_Resp,
        CPU_Store_Resp,
        GetSL1,
        GetML1,
        GetML1_WT,
        GetML1_Ack,
        GetSL1_Ack_Data,
        PutML1,
        Put_AckL1, 
        Fwd_GetML1,
        -- for RRC
        Notify,
        Req_Notify
    };

    ENTRY_cacheL1: record
        State: L1Perm;
        cl: ClValue;
        requestor: OBJSET_CPU;
    end;

    MACH_cacheL1: record
        cb: array[Address] of ENTRY_cacheL1;
    end;    

    ENTRY_dir: record
        State: DirPerm;
        cl: ClValue;
        ownerL1: OBJSET_cacheL1;
    end;

    MACH_dir: record
        cb: array[Address] of ENTRY_dir;
        -- bellow are for RRC
        TSToStCnt: array[OBJSET_CPU] of array[RRCTimestamp] of RRCCounter;
        -- no need for pendingcompnot, just keep it in the netbuf
        compNotiCnt: array[OBJSET_CPU] of array[RRCTimestamp] of DirCnt;
        -- uncommitedStTS: array[OBJSET_CPU] of T_uncommitedTS;
        maxCommitedTS: array[OBJSET_CPU] of RRCTimestamp;
    end;
    
    INSTR: record
      access: AccType;
      cst: AccCst;
      adr: Address;
      val: ClValue;
      -- exp_val: ClValue;
      done: boolean;
    end;

    INSTR_QUEUE: record
        Queue: array[InstrCnt] of INSTR;
        CurIdx: InstrCnt;
        Cnt: InstrCnt;
    end;

    MACH_CPU: record
        cache: OBJSET_cacheL1;
        instrs: INSTR_QUEUE;
        -- below are for RRC
        myTS: RRCTimestamp;
        myStCnt: array[OBJSET_dir] of RRCCounter;
        uncommitedDirToTS: array[OBJSET_dir] of T_uncommitedTS;
        inReclaim: boolean;
    end;

    Message: record
        adr: Address;
        mtype: MessageType;
        cst: AccCst;
        instr_idx: InstrCnt;
        src: Machines;
        dst: Machines;
        cl: ClValue;
        -- for RRC
        TS: RRCTimestamp;
        maxUncommitedTS: RRCTimestamp;
        stCnt: RRCCounter;
        dirWaitCnt: DirCnt;
        dstOrSrc: Machines;
    end;

    OBJ_CPU: array[OBJSET_CPU] of MACH_CPU;

    OBJ_L1: array[OBJSET_cacheL1] of MACH_cacheL1;

    OBJ_dir: array[OBJSET_dir] of MACH_dir;

    NET_Unordered: array [Machines] of multiset[U_NET_MAX] of Message;

var
    cpus: OBJ_CPU;
    cacheL1s: OBJ_L1;
    dirs: OBJ_dir;

    req: NET_Unordered;
    fwd: NET_Unordered;
    resp: NET_Unordered;

    procedure Reset_network();
    begin
      undefine fwd;
      undefine req;
      undefine resp;
    end;

    procedure Reset_machine_dir();
    begin
      for i:OBJSET_dir do
        for a:Address do
          dirs[i].cb[a].State := Dir_I;
          dirs[i].cb[a].cl := 0;
          undefine dirs[i].cb[a].ownerL1;
        endfor;
      endfor;
    end;

    procedure Reset_machine_cacheL1();
    begin
      for i:OBJSET_cacheL1 do
        for a:Address do
          cacheL1s[i].cb[a].State := L1_I;
          cacheL1s[i].cb[a].cl := 0;
        endfor;
      endfor;
    end;

    procedure Reset_CPU();
    begin
        for i:OBJSET_CPU do
            undefine cpus[i].cache;
            undefine cpus[i].instrs.Queue;
            cpus[i].instrs.CurIdx:=0;
            cpus[i].instrs.Cnt:=0;
        endfor;
    end;

    function CPU_Next(cur: OBJSET_CPU): OBJSET_CPU;
    begin
			if cur = CPU0 then
				return CPU1;
			endif;
			if cur = CPU1 then
				return CPU0;
			endif;
			return CPU0
    end;

    procedure CPU_Cache_Map();
    var CPU_ind: OBJSET_CPU;
    begin
        CPU_ind := CPU0;
        for i:OBJSET_cacheL1 do
            cpus[CPU_ind].cache := i;
            CPU_ind := CPU_Next(CPU_ind);
        endfor;
    end;

    procedure Reset_machine();
    begin
        Reset_machine_dir();
        Reset_machine_cacheL1();
        Reset_CPU();
    end;

    procedure Add_Instr();
    begin
      /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


alias cpu:cpus[CPU0] do
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[0] do
            instr.access := store;
            instr.cst := RLX;
            instr.adr := 0;
            instr.val := 1;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[1] do
            instr.access := store;
            instr.cst := REL;
            instr.adr := 1;
            instr.val := 1;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[2] do
            instr.access := store;
            instr.cst := RLX;
            instr.adr := 2;
            instr.val := 1;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[3] do
            instr.access := store;
            instr.cst := REL;
            instr.adr := 3;
            instr.val := 1;
            instr.done := false;
        endalias;
        endalias;
        cpu.instrs.Cnt := 4
        endalias;

			/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


alias cpu:cpus[CPU1] do
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[0] do
            instr.access := load;
            instr.cst := ACQ;
            instr.adr := 3;
            instr.val := 0;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[1] do
            instr.access := load;
            instr.cst := RLX;
            instr.adr := 2;
            instr.val := 0;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[2] do
            instr.access := load;
            instr.cst := ACQ;
            instr.adr := 1;
            instr.val := 0;
            instr.done := false;
        endalias;
        endalias;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


        alias q: cpu.instrs.Queue do
        alias instr: q[3] do
            instr.access := load;
            instr.cst := RLX;
            instr.adr := 0;
            instr.val := 0;
            instr.done := false;
        endalias;
        endalias;
        cpu.instrs.Cnt := 4
        endalias;
    end;

    procedure Litmus_CPU_Init();
    begin
        Reset_CPU();
        CPU_Cache_Map();
        Add_Instr();
    end;

    procedure Reset_RRC_CPU();
    begin
      for i:OBJSET_CPU do
        alias cpu: cpus[i] do
          cpu.myTS := 0;
          cpu.inReclaim := false;
          -- cpu.inTSReclaim := false;
          for dir: OBJSET_dir do
            cpu.myStCnt[dir] := 0;
            for t:=0 to TS_MAX do
              cpu.uncommitedDirToTS[dir][t] := true; -- true always mean commited, or not issued
            endfor;
          endfor;
        endalias;
      endfor;
    end;

   procedure Reset_RRC_dir();
    begin
      for i:OBJSET_dir do
        alias dir: dirs[i] do
          for c: OBJSET_CPU do
            for t:=0 to TS_MAX do
              dir.TSToStCnt[c][t] := 0;
              dir.compNotiCnt[c][t] := 0;
              -- dir.uncommitedStTS[c][t] := true;
              undefine dir.maxCommitedTS[c];
            endfor;
          endfor;
        endalias;
      endfor;
    end;

    procedure Reset_RRC_meta();
    begin
      Reset_RRC_CPU();
      Reset_RRC_dir();
    end;

    procedure System_Reset();
    begin
        Reset_network();
        Reset_machine();
        Litmus_CPU_Init();
        Reset_RRC_meta();
    end;

    procedure Send_req(msg: Message);
    begin
        Assert (MultiSetCount(i:req[msg.dst], true) < U_NET_MAX) "Too many messages";
        MultisetAdd(msg, req[msg.dst]);
    end;

    procedure Send_resp(msg: Message);
    begin
        Assert (MultiSetCount(i:resp[msg.dst], true) < U_NET_MAX) "Too many messages";
        MultisetAdd(msg, resp[msg.dst]);
    end;

    procedure Send_fwd(msg:Message);
    begin
        Assert (MultiSetCount(i:fwd[msg.dst], true) < U_NET_MAX) "Too many messages";
        MultiSetAdd(msg, fwd[msg.dst]);
    end;

    function NextInstr(var cpu: MACH_CPU): INSTR;
    var instr: INSTR;
    begin
      alias p: cpu.instrs do
      alias q: p.Queue do
      alias qidx: p.CurIdx do
      alias qcnt: p.Cnt do
      undefine instr;
    
      -- if qind = qcnt then
        -- return instr;
      -- endif;
      -- Assert(qidx < qcnt);
    
      if !isundefined(q[qidx].access) then
        -- q[qind].pend := true;   /* Set instruction as active */
        instr := q[qidx];
        qidx := qidx + 1;
      endif;
    
      return instr;
    
      endalias;
      endalias;
      endalias;
      endalias;
    end;

    function ReqCPUToL1(adr: Address; mtype: MessageType; cst: AccCst; instr_idx: InstrCnt; src: Machines; dst: Machines; cl: ClValue): Message;
    var m: Message;
    begin
        m.adr := adr;
        m.mtype := mtype;
        m.cst := cst;
        m.instr_idx := instr_idx;
        m.src := src;
        m.dst := dst;
        m.cl := cl;
        return m;
    end;

    function RespL1ToCPU(adr: Address; mtype: MessageType; cst: AccCst; instr_idx: InstrCnt; src: Machines; dst: Machines; cl: ClValue): Message;
    var m: Message;
    begin
        m.adr := adr;
        m.mtype := mtype;
        m.cst := cst;
        m.instr_idx := instr_idx;
        m.src := src;
        m.dst := dst;
        m.cl := cl;
        return m;
    end;

    function ReqL1ToDir(adr: Address; mtype: MessageType; cst: AccCst; instr_idx: InstrCnt; src: Machines; dst: Machines; cl: ClValue) : Message;
    var m: Message;
    begin
      m.adr := adr;
      m.mtype := mtype;
      m.cst := cst;
      m.instr_idx := instr_idx;
      m.src := src;
      m.dst := dst;
      m.cl := cl;
      return m;
    end;

    function RespDirToL1(adr: Address; mtype: MessageType; cst: AccCst; instr_idx: InstrCnt; src: Machines; dst: Machines; cl: ClValue) : Message;
    var m: Message;
    begin
      m.adr := adr;
      m.mtype := mtype;
      m.cst := cst;
      m.instr_idx := instr_idx;
      m.src := src;
      m.dst := dst;
      m.cl := cl;
      return m;
    end;


    function AddrToDir(adr: Address): OBJSET_dir;
    begin
			if adr = 0 then
				return DIR1;
			endif;
			if adr = 1 then
				return DIR1;
			endif;
			if adr = 2 then
				return DIR1;
			endif;
			if adr = 3 then
				return DIR0;
			endif;
			return DIR0
    end;

    function AccToMtype(access: AccType): MessageType;
    var mtype: MessageType;
    begin
        undefine mtype;
        switch access
            case load: mtype := CPU_Load;
            case store: mtype := CPU_Store;
            case nt_store: mtype := CPU_Ntstore;
            case self_inv: mtype := CPU_Selfinv;
            case barrier: mtype := CPU_Barrier;
        endswitch;
        return mtype;
    end;

    function AccToTranState(access: AccType): L1Perm;
    var next_state: L1Perm;
    begin
        undefine next_state;
        switch access
            case load: next_state := L1_I_load;
            case store: next_state := L1_I_store;
            case nt_store: next_state := L1_I;
            case self_inv: next_state := L1_I;
        endswitch;
        return next_state;
    end;

    procedure RRC_CPU_get_max_uncommited_TS(TS_list: T_uncommitedTS; curTS: RRCTimestamp; var max: RRCTimestamp);
    begin
      undefine max;
      for t:=0 to curTS do
        alias commited : TS_list[t] do
          if !commited then
            max := t;
          endif;
        endalias;
      endfor;
    end;

    procedure RRC_dir_get_min_uncommited_TS(TS_list: T_uncommitedTS; var min: RRCTimestamp);
    begin
      undefine min;
      for t:=0 to TS_MAX do
        alias commited : TS_list[t] do
          if !commited then
            min := t;
            return;
          endif;
        endalias;
      endfor;
    end;

    procedure RRC_CPU_handle_barrier_req(m: OBJSET_CPU; var cpu: MACH_CPU; instr: INSTR; instr_idx: InstrCnt);
    var barrier: Message;
    var maxUncommitedTS: RRCTimestamp;
    begin
      Assert(instr.cst = SEQ);
      for dir:OBJSET_dir do
        RRC_CPU_get_max_uncommited_TS(cpu.uncommitedDirToTS[dir], cpu.myTS, maxUncommitedTS);
        if (cpu.myStCnt[dir] > 0) | !isundefined(maxUncommitedTS) then
          barrier.mtype := CPU_Barrier;
          barrier.src := m;
          barrier.dst := dir;
          barrier.instr_idx := instr_idx;
          barrier.TS := cpu.myTS;
          barrier.stCnt := cpu.myStCnt[dir];
          barrier.dstOrSrc := m;
          barrier.dirWaitCnt := 0;
          Send_req(barrier);
        endif;
      endfor;
      cpu.myTS := cpu.myTS + 1;
      for i:OBJSET_dir do
        cpu.myStCnt[i] := 0;
      endfor;
    end;

    procedure RRC_CPU_handle_store_req(m: OBJSET_CPU; var cpu: MACH_CPU; instr: INSTR; var req: Message);
    var dirWaitCnt: DirCnt;
    var req_notify: Message;
    begin
      if instr.cst = RLX then
        alias dir_idx: AddrToDir(instr.adr) do
          cpu.myStCnt[dir_idx] := cpu.myStCnt[dir_idx] + 1;
          req.TS := cpu.myTS;
        endalias;
      elsif instr.cst = REL then
        alias dir_idx: AddrToDir(instr.adr) do
          dirWaitCnt := 0;
          for i:OBJSET_dir do
            if i != dir_idx then
              RRC_CPU_get_max_uncommited_TS(cpu.uncommitedDirToTS[i], cpu.myTS, req_notify.maxUncommitedTS);
              if cpu.myStCnt[i] > 0 | !isundefined(req_notify.maxUncommitedTS) then
                req_notify.mtype := Req_Notify;
                req_notify.src := m;
                req_notify.dst := i;
                req_notify.TS := cpu.myTS;
                -- RRC_CPU_get_max_uncommited_TS(cpu.uncommitedDirToTS[i], cpu.myTS, req_notify.maxUncommitedTS);
                req_notify.stCnt := cpu.myStCnt[i];
                req_notify.dstOrSrc := dir_idx;
                Send_req(req_notify);
                dirWaitCnt := dirWaitCnt + 1;
              endif;
            endif;
          endfor;

          req.TS := cpu.myTS;
          req.stCnt := cpu.myStCnt[dir_idx];
          RRC_CPU_get_max_uncommited_TS(cpu.uncommitedDirToTS[dir_idx], cpu.myTS, req.maxUncommitedTS);
          req.dirWaitCnt := dirWaitCnt;

          cpu.uncommitedDirToTS[dir_idx][cpu.myTS] := false; -- false means not commited
          cpu.myTS := cpu.myTS + 1;
          for i:OBJSET_dir do
            cpu.myStCnt[i] := 0;
          endfor;
        endalias;
      endif;
    end;

    procedure Issue_CPU(var cpu: MACH_CPU; cpu_idx: OBJSET_CPU);
    var instr: INSTR;
    var msg: Message;
    var mtype: MessageType;
    begin
        instr := NextInstr(cpu);
        alias adr: instr.adr do
        alias m: cpu.cache do
        alias cbe: cacheL1s[m].cb[adr] do
        alias access: instr.access do
        alias cst: instr.cst do
        alias val: instr.val do

        -- for RRC
        if access = barrier then
          RRC_CPU_handle_barrier_req(cpu_idx, cpu, instr, cpu.instrs.CurIdx-1);
          return;
        endif;

        mtype := AccToMtype(access);
        msg := ReqCPUToL1(adr, mtype, cst, cpu.instrs.CurIdx-1, cpu_idx, m, val);
        
        -- for RRC
        if access = store | access = nt_store then
          RRC_CPU_handle_store_req(cpu_idx, cpu, instr, msg);
        endif;

        Send_req(msg);

        -- for RRC
        if access = nt_store then
          cpu.instrs.Queue[cpu.instrs.CurIdx-1].done := true;
        endif;

        endalias;
        endalias;
        endalias;
        endalias;
        endalias;
        endalias;
    end;

  
    function FSM_MSG_cacheL1(inmsg:Message; m:OBJSET_cacheL1) : boolean;
    var msg: Message;
    -- support ntstore, just send msg to dir, no state transition, no expecting ack
    -- support load, just send msg to dir, no state transition, expect data
    -- support store, just send msg to dir, not state transition, expect ack
    begin
      alias adr: inmsg.adr do
      alias cbe: cacheL1s[m].cb[adr] do
      switch cbe.State

        case L1_I:
          switch inmsg.mtype
            case CPU_Load:
              msg := ReqL1ToDir(inmsg.adr, GetSL1, inmsg.cst, inmsg.instr_idx, m, AddrToDir(inmsg.adr), inmsg.cl);
              Send_req(msg);
              cbe.State := L1_I_load;
              cbe.requestor := inmsg.src;
              return true;
            case CPU_Store:
              msg := ReqL1ToDir(inmsg.adr, GetML1, inmsg.cst, inmsg.instr_idx, m, AddrToDir(inmsg.adr), inmsg.cl);
              -- for RRC
              msg.dstOrSrc := inmsg.src;
              msg.TS := inmsg.TS;
              if inmsg.cst = REL then
                msg.stCnt := inmsg.stCnt;
                msg.dirWaitCnt := inmsg.dirWaitCnt;
                msg.maxUncommitedTS:= inmsg.maxUncommitedTS
              endif;
              --
              Send_req(msg);
              cbe.State := L1_I_store;
              cbe.requestor := inmsg.src;
              return true;
            case CPU_Ntstore:
              msg := ReqL1ToDir(inmsg.adr, GetML1_WT, inmsg.cst, inmsg.instr_idx, m, AddrToDir(inmsg.adr), inmsg.cl);
              -- for RRC
              msg.dstOrSrc := inmsg.src;
              msg.TS := inmsg.TS;
              Assert(inmsg.cst != REL); -- don't support nt rel yet
              --
              Send_req(msg);
              cbe.State := L1_I; -- state not changed
              return true;
            else return false;
          endswitch;

        case L1_I_load:
          switch inmsg.mtype
            case GetSL1_Ack_Data:
              msg := RespL1ToCPU(inmsg.adr, CPU_Load_Resp, inmsg.cst, inmsg.instr_idx, m, cbe.requestor, inmsg.cl);
              Send_resp(msg);
              cbe.State := L1_I; -- for simplicity, no caching
              return true;
            else return false;
          endswitch;
  
        case L1_I_store:
          switch inmsg.mtype
            case GetML1_Ack:
              msg := RespL1ToCPU(inmsg.adr, CPU_Store_Resp, inmsg.cst, inmsg.instr_idx, m, cbe.requestor, inmsg.cl);
              -- for RRC
              if inmsg.cst = REL then
                msg.TS := inmsg.TS;
                msg.dstOrSrc := inmsg.src;
              endif;
              --
              Send_resp(msg);
              cbe.State := L1_I;
              return true;
            else return false;
          endswitch;

        else return false;

      endswitch;

      endalias;
      endalias;
      return false;
    end;

    function canCommit(inmsg:Message; dir:MACH_dir) : boolean ;
    -- var minUncommitedTS: RRCTimestamp;
    begin
      -- RRC_dir_get_min_uncommited_TS(dir.uncommitedStTS[inmsg.dstOrSrc], minUncommitedTS);
      return (dir.TSToStCnt[inmsg.dstOrSrc][inmsg.TS] = inmsg.stCnt)
        & (dir.compNotiCnt[inmsg.dstOrSrc][inmsg.TS] = inmsg.dirWaitCnt)
        -- & (isundefined(minUncommitedTS) | minUncommitedTS >= inmsg.TS);
        & (isundefined(inmsg.maxUncommitedTS) | (!isundefined(dir.maxCommitedTS[inmsg.dstOrSrc]) & dir.maxCommitedTS[inmsg.dstOrSrc] >= inmsg.maxUncommitedTS));
    end;

    function canSendNotify(inmsg:Message; dir:MACH_dir) : boolean ;
    var minUncommitedTS: RRCTimestamp;
    begin
      -- RRC_dir_get_min_uncommited_TS(dir.uncommitedStTS[inmsg.src], minUncommitedTS);
      return (dir.TSToStCnt[inmsg.src][inmsg.TS] = inmsg.stCnt)
        -- & (isundefined(inmsg.maxUncommitedTS) | isundefined(minUncommitedTS) | (minUncommitedTS > inmsg.maxUncommitedTS))
        & (isundefined(inmsg.maxUncommitedTS) | (!isundefined(dir.maxCommitedTS[inmsg.src]) & dir.maxCommitedTS[inmsg.src] >= inmsg.maxUncommitedTS));
    end;

    function canReclaim(inmsg:Message; dir:MACH_dir) : boolean;
    begin
      return (dir.TSToStCnt[inmsg.src][inmsg.TS] >= inmsg.stCnt);
    end;
    
    function FSM_MSG_dir(inmsg:Message; m:OBJSET_dir) : boolean;
    var msg: Message;
    var notify: Message;
    var bar_resp: Message;
    var reclaim_resp: Message;
    -- support ntstore, commit to l2, do not send ack
    -- support store, commit to l2, send ack
    -- support read, send back data
    begin
      alias dir: dirs[m] do
      -- for RRC
      if (inmsg.mtype = GetML1 | inmsg.mtype = GetML1_WT) then
        if inmsg.cst = REL then
          if !canCommit(inmsg, dir) then
            -- dir.uncommitedStTS[inmsg.dstOrSrc][inmsg.TS] := false;
            return false;
          else
            dir.TSToStCnt[inmsg.dstOrSrc][inmsg.TS] := 0;
            dir.compNotiCnt[inmsg.dstOrSrc][inmsg.TS] := 0;
            -- dir.uncommitedStTS[inmsg.dstOrSrc][inmsg.TS] := true;
            dir.maxCommitedTS[inmsg.dstOrSrc] := inmsg.TS;
          endif;
        elsif inmsg.cst = RLX then
          dir.TSToStCnt[inmsg.dstOrSrc][inmsg.TS] := dir.TSToStCnt[inmsg.dstOrSrc][inmsg.TS] + 1;
        endif;
      elsif inmsg.mtype = Req_Notify then
        if !canSendNotify(inmsg, dir) then
          return false;
        else
          dir.TSToStCnt[inmsg.src][inmsg.TS] := 0;
          notify.mtype := Notify;
          notify.src := m;
          notify.dst := inmsg.dstOrSrc; -- notify dst
          notify.TS := inmsg.TS;
          notify.dstOrSrc := inmsg.src; -- src of the instr
          Send_req(notify);
          return true;
        endif;
      elsif inmsg.mtype = Notify then
        dir.compNotiCnt[inmsg.dstOrSrc][inmsg.TS] := dir.compNotiCnt[inmsg.dstOrSrc][inmsg.TS] + 1;
        return true;
      elsif inmsg.mtype = CPU_Barrier then
        if !canCommit(inmsg, dir) then
          -- dir.uncommitedStTS[inmsg.dstOrSrc][inmsg.TS] := false;
          return false;
        else
          dir.TSToStCnt[inmsg.dstOrSrc][inmsg.TS] := 0;
          dir.compNotiCnt[inmsg.dstOrSrc][inmsg.TS] := 0;
          -- dir.uncommitedStTS[inmsg.dstOrSrc][inmsg.TS] := true;
          dir.maxCommitedTS[inmsg.dstOrSrc] := inmsg.TS;
          bar_resp.mtype := CPU_Barrier_Resp;
          bar_resp.src := m;
          bar_resp.dst := inmsg.src;
          bar_resp.TS := inmsg.TS;
          bar_resp.instr_idx := inmsg.instr_idx;
          Send_resp(bar_resp);
          return true;
        endif;
      elsif inmsg.mtype = CPU_RRC_Reclaim then
        if !canReclaim(inmsg, dir) then
          return false;
        else
          dir.TSToStCnt[inmsg.src][inmsg.TS] := 0;
          for c:OBJSET_CPU do
            -- for t:=0 to TS_MAX do
              -- Assert(dir.uncommitedStTS[c][t] = true);
            undefine dir.maxCommitedTS[c];
            -- endfor;
          endfor;
          reclaim_resp.mtype := CPU_RRC_Reclaim_Resp;
          reclaim_resp.src := m;
          reclaim_resp.dst := inmsg.src;
          Send_resp(reclaim_resp);
          return true;
        endif;
      endif;

      alias adr: inmsg.adr do
      alias cbe: dirs[m].cb[adr] do
      switch cbe.State
        case Dir_I:
          switch inmsg.mtype
          case GetSL1:
            msg := RespDirToL1(adr, GetSL1_Ack_Data, inmsg.cst, inmsg.instr_idx, m, inmsg.src, cbe.cl);
            Send_resp(msg);
            return true;
          case GetML1:
            cbe.cl := inmsg.cl;
            msg := RespDirToL1(adr, GetML1_Ack, inmsg.cst, inmsg.instr_idx, m, inmsg.src, inmsg.cl);
            -- for RRC
            if inmsg.cst = REL then
              msg.dstOrSrc := m;
              msg.TS := inmsg.TS;
            endif;
            --
            Send_resp(msg);
            return true;
          case GetML1_WT:
            -- for RRC
            Assert(inmsg.cst != REL); -- currently only support all release store being temporal.
            --
            cbe.cl := inmsg.cl;
            return true;
          else return false;
        endswitch;
        else return false;
      endswitch;
      endalias;
      endalias;
      endalias;
      return false;
    end;

    procedure RetireInstr(var cpu: MACH_CPU; idx: InstrCnt; val: ClValue);
    begin
      alias p: cpu.instrs do
      alias q: p.Queue do

      q[idx].val := val;
      q[idx].done := true;

      endalias;
      endalias;
    end;

    function FSM_MSG_CPU(inmsg:Message; m:OBJSET_CPU) : boolean;
    var bar_all_committed: boolean;
    begin
      -- alias adr: inmsg.adr do
      switch inmsg.mtype
        case CPU_Load_Resp:
          RetireInstr(cpus[m], inmsg.instr_idx, inmsg.cl);
          return true;
        case CPU_Store_Resp:
          -- for RRC
          if inmsg.cst = REL then
            cpus[m].uncommitedDirToTS[inmsg.dstOrSrc][inmsg.TS] := true;
          endif;
          --
          RetireInstr(cpus[m], inmsg.instr_idx, inmsg.cl);
          return true;
        case CPU_Barrier_Resp:
          -- for RRC
          cpus[m].uncommitedDirToTS[inmsg.src][inmsg.TS] := true;
          bar_all_committed := true;
          for dir:OBJSET_dir do
            if !cpus[m].uncommitedDirToTS[dir][inmsg.TS] then
              bar_all_committed := false;
            endif;
          endfor;
          if bar_all_committed then
            RetireInstr(cpus[m], inmsg.instr_idx, 0);
          endif;
          return true;
        case CPU_RRC_Reclaim_Resp:
          cpus[m].myStCnt[inmsg.src] := 0;
          bar_all_committed := true;
          for dir: OBJSET_dir do
            if isundefined(cpus[m].myStCnt[dir]) then
              bar_all_committed := false;
            endif;
          endfor;
          if bar_all_committed then
            cpus[m].myTS := 0;
            cpus[m].inReclaim := false;
          endif;
          return true;
        else return false;
      endswitch;
      -- endalias;
      return false;
    end;

    -- FSM_MSG CPU, should only expect data for load

    function Can_Issue_Next_Instr(cpu: MACH_CPU) : boolean;
    var i: InstrCnt;
    begin
      alias p: cpu.instrs do
      alias q: p.Queue do
      alias cur: q[p.CurIdx] do

      if p.CurIdx = 0 then
        return true;
      elsif p.CurIdx = p.Cnt then
        return false;
      endif;

      -- for RRC relaim
      if cpu.inReclaim then
        return false;
      endif;

      for i := 0 to p.CurIdx-1 do
        alias prev: q[i] do
          if (cur.adr = prev.adr) & (!prev.done) then
            return false;
          elsif cur.cst = REL & (!prev.done) & (prev.access = load) then
            return false;
          elsif (prev.cst = ACQ) & (!prev.done) then
            return false;
          elsif (prev.access = barrier) & (!prev.done) then
            return false;
          elsif (cur.access = barrier) & (!prev.done) then
            return false;
          -- for RRC TS and Cnt overflow
          elsif (cpu.myTS = TS_MAX) & (cur.cst = REL | cur.access = barrier) then -- TS overflow
            return false;
          elsif (cur.access = store | cur.access = nt_store) & (cur.cst = RLX) & (cpu.myStCnt[AddrToDir(cur.adr)] = CNT_MAX) then -- Cnt overflow
            return false;
          endif;
        endalias;
      endfor;

      return true;

      -- if q.CurIdx < q.Cnt then
      --   return true;
      -- else return false;
      -- endif;

      endalias;
      endalias;
      endalias;
    end;

    function All_CPU_done() : boolean;
    begin
      for i:OBJSET_CPU do
        alias cpu: cpus[i] do
        alias q: cpu.instrs do
        for j := 0 to q.Cnt-1 do
          if !q.Queue[j].done then
            return false;
          endif;
        endfor;
        endalias;
        endalias;
      endfor;
      return true;
    end;

    function Forbidden(): boolean ;
    -- var match_cnt: 0..LOAD_COUNT;
    -- var load_cnt: 0..LOAD_COUNT;
    begin
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


    if /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[0].val = 1 & /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[1].val = 0 then
        return false;
    endif;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


    if /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[0].val = 1 & /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[2].val = 0 then
        return false;
    endif;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


    if /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[0].val = 1 & /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[3].val = 0 then
        return false;
    endif;
/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


    if /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[2].val = 1 & /*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


cpus[CPU1].instrs.Queue[3].val = 0 then
        return false;
    endif;
		return true;
      -- match_cnt := 0;
      -- for i:OBJSET_CPU do
      --   alias cpu: cpus[i] do
      --   alias q: cpu.instrs.Queue do
      --   for j := 0 to q.Cnt-1 do
      --     if q[j].acc_type = load then
      --       load_cnt := load_cnt + 1;
      --       if q[j].val = q[j].exp_val then
      --         match_cnt := match_cnt + 1;
      --       endif;
      --     endif;
      --   endfor;
      --   endalias;
      --   endalias;
      -- endfor;

      -- if match_cnt != load_cnt then
      --   error "Litmus Test Failed"
      -- endif;
    end;

    function Expected(): boolean;
    begin

		return true;
    end;

    procedure cacheL1_M_evict(adr:Address; m:OBJSET_cacheL1);
    var msg: Message;
    begin
    alias cbe:cacheL1s[m].cb[adr] do
      msg := ReqL1ToDir(adr, PutML1, RLX, 0, m, AddrToDir(adr), cbe.cl);
      Send_req(msg);
      cbe.State := L1_M_evict;
    endalias;
    end;

    function RRC_should_reclaim(cpu: MACH_CPU; m: OBJSET_CPU): boolean;
    var maxUncommitedTS: RRCTimestamp;
    begin
        if !cpu.inReclaim then
          if cpu.instrs.CurIdx < cpu.instrs.Cnt then
          alias cur: cpu.instrs.Queue[cpu.instrs.CurIdx] do
          alias dir: AddrToDir(cur.adr) do
            if ((cur.access = store | cur.access = nt_store) & (cur.cst = RLX) & (cpu.myStCnt[dir] = CNT_MAX)) -- cnt overflow
              | ((cpu.myTS = TS_MAX) & (cur.cst = REL | cur.access = barrier)) then -- TS overflow
              for d: OBJSET_dir do -- make sure all prior TS has commited
                RRC_CPU_get_max_uncommited_TS(cpu.uncommitedDirToTS[d], cpu.myTS, maxUncommitedTS);
                if !isundefined(maxUncommitedTS) then
                  return false;
                endif;
              endfor;
              return true;
            endif;
          endalias;
          endalias;
          endif;
        endif;
      return false;
    end;

    procedure RRC_reclaim(var cpu: MACH_CPU; m: OBJSET_CPU);
    var reclaim: Message;
    begin
      alias cur: cpu.instrs.Queue[cpu.instrs.CurIdx] do
        cpu.inReclaim := true;
        for dir: OBJSET_dir do
          reclaim.mtype := CPU_RRC_Reclaim;
          reclaim.src := m;
          reclaim.dst := dir;
          reclaim.TS := cpu.myTS;
          reclaim.stCnt := cpu.myStCnt[dir];
          Send_req(reclaim);
          undefine cpu.myStCnt[dir];
        endfor;
      endalias;
    end;

    ruleset m:OBJSET_CPU do
        alias cpu: cpus[m] do

        rule "CPU_serve"
            Can_Issue_Next_Instr(cpu)
        ==>
            Issue_CPU(cpus[m], m);
        endrule;
    
        /* RULESET RESET CONDITION */
        rule "CPU_done"
            All_CPU_done()
        ==>
          /* Evaluate invariants */
          if !Forbidden() then
            error "Litmus Test Failed";
          endif;

          if !Expected() then
            error "Litmus Test Failed";
          endif;
            
          /* Then perform reset */
          System_Reset();
        endrule;

        endalias;
    endruleset;

    ruleset m:OBJSET_CPU do
      alias cpu: cpus[m] do

      rule "reclaim counter timestamp"
        RRC_should_reclaim(cpu, m)
      ==>
        RRC_reclaim(cpu, m);
      endrule;
      
      endalias;
    endruleset;

    ruleset m:OBJSET_cacheL1 do
        ruleset adr:Address do
            alias cbe:cacheL1s[m].cb[adr] do
            rule "cacheL1_evict"
                cbe.State = L1_M
            ==>
                cacheL1_M_evict(adr, m);
            endrule;
            endalias;
        endruleset;
    endruleset;

  ruleset dst:Machines do
        choose midx:fwd[dst] do
            alias mach:fwd[dst] do
            alias msg:mach[midx] do
              rule "Receive fwd"
                !isundefined(msg.mtype)
              ==>
            if IsMember(dst, OBJSET_dir) then
              if FSM_MSG_dir(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_cacheL1) then
              if FSM_MSG_cacheL1(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_CPU) then
              if FSM_MSG_CPU(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            else error "unknown machine";
            endif;
    
              endrule;
            endalias;
            endalias;
        endchoose;
    endruleset;
    
    ruleset dst:Machines do
        choose midx:req[dst] do
            alias mach:req[dst] do
            alias msg:mach[midx] do
              rule "Receive req"
                !isundefined(msg.mtype)
              ==>
            if IsMember(dst, OBJSET_dir) then
              if FSM_MSG_dir(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_cacheL1) then
              if FSM_MSG_cacheL1(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_CPU) then
              if FSM_MSG_CPU(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            else error "unknown machine";
            endif;
    
              endrule;
            endalias;
            endalias;
        endchoose;
    endruleset;
    
    ruleset dst:Machines do
        choose midx:resp[dst] do
            alias mach:resp[dst] do
            alias msg:mach[midx] do
              rule "Receive resp"
                !isundefined(msg.mtype)
              ==>
            if IsMember(dst, OBJSET_dir) then
              if FSM_MSG_dir(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_cacheL1) then
              if FSM_MSG_cacheL1(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            elsif IsMember(dst, OBJSET_CPU) then
              if FSM_MSG_CPU(msg, dst) then
                MultiSetRemove(midx, mach);
              endif;
            else error "unknown machine";
            endif;
    
              endrule;
            endalias;
            endalias;
        endchoose;
    endruleset;

    startstate
        System_Reset();
    endstartstate;
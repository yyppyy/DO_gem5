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
    VAL_COUNT: {{VAL_COUNT}};
    ADR_COUNT: {{ADR_COUNT}};

    U_NET_MAX: {{U_NET_MAX}};

    CPU_COUNT: {{CPU_COUNT}};
    CACHE_L1_COUNT: {{CACHE_L1_COUNT}};
    DIR_COUNT: {{DIR_COUNT}};
    INSTR_COUNT: {{INSTR_COUNT}};
    -- LOAD_COUNT: {{LOAD_COUNT}}
    -- ADDR_PER_DIR: ADDR_COUNT / DIR_COUNT;

type
    Address: 0..ADR_COUNT-1;
    ClValue: 0..VAL_COUNT-1;
    InstrCnt: 0..INSTR_COUNT;
    OBJSET_CPU: enum{ {{ENUM_CPU_LIST}} };
    OBJSET_cacheL1: scalarset(CACHE_L1_COUNT);
    OBJSET_dir: enum{ {{ENUM_DIR_LIST}} };
    Machines: union{OBJSET_CPU, OBJSET_cacheL1, OBJSET_dir};

    AccType: enum {
        load,
        store,
        nt_store,
        self_inv, 
        none
    };

    AccCst: enum {
        RLX,
        ACQ,
        REL
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
        CPU_Load_Resp,
        CPU_Store_Resp,
        GetSL1,
        GetML1,
        GetML1_WT,
        GetML1_Ack,
        GetSL1_Ack_Data,
        PutML1,
        Put_AckL1, 
        Fwd_GetML1
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
        instrs: INSTR_QUEUE
    end;

    Message: record
        adr: Address;
        mtype: MessageType;
        cst: AccCst;
        instr_idx: InstrCnt;
        src: Machines;
        dst: Machines;
        cl: ClValue;
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
{{FUNC_BODY_CPU_NEXT}}
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
--     var cache: OBJSET_cacheL1;
--     begin
-- {{FUNC_BODY_CPU_CACHE_MAP}}
--     end;

    procedure Reset_machine();
    begin
        Reset_machine_dir();
        Reset_machine_cacheL1();
        Reset_CPU();
    end;

    procedure Add_Instr();
    begin
      {{FUNC_BODY_ADD_CPU_INSTR}}
    end;

    procedure Litmus_CPU_Init();
    begin
        Reset_CPU();
        CPU_Cache_Map();
        Add_Instr();
    end;

    procedure System_Reset();
    begin
        Reset_network();
        Reset_machine();
        Litmus_CPU_Init();
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
{{FUNC_BODY_ADDR_TO_DIR}}
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

        mtype := AccToMtype(access);
        msg := ReqCPUToL1(adr, mtype, cst, cpu.instrs.CurIdx-1, cpu_idx, m, val);
        Send_req(msg);

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
              Send_req(msg);
              cbe.State := L1_I_store;
              cbe.requestor := inmsg.src;
              return true;
            case CPU_Ntstore:
              msg := ReqL1ToDir(inmsg.adr, GetML1_WT, inmsg.cst, inmsg.instr_idx, m, AddrToDir(inmsg.adr), inmsg.cl);
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
    
    function FSM_MSG_dir(inmsg:Message; m:OBJSET_dir) : boolean;
    var msg: Message;
    -- support ntstore, commit to l2, do not send ack
    -- support store, commit to l2, send ack
    -- support read, send back data
    begin
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
            Send_resp(msg);
            return true;
          case GetML1_WT:
            cbe.cl := inmsg.cl;
            return true;
          else return false;
        endswitch;
        else return false;
      endswitch;
      endalias;
      endalias;
      return false;
    end;

    procedure PopInstr(var cpu: MACH_CPU; idx: InstrCnt; val: ClValue);
    begin
      alias p: cpu.instrs do
      alias q: p.Queue do

      q[idx].val := val;
      q[idx].done := true;

      endalias;
      endalias;
    end;

    function FSM_MSG_CPU(inmsg:Message; m:OBJSET_CPU) : boolean;
    begin
      alias adr: inmsg.adr do
      switch inmsg.mtype
        case CPU_Load_Resp:
          PopInstr(cpus[m], inmsg.instr_idx, inmsg.cl);
        case CPU_Store_Resp:
          PopInstr(cpus[m], inmsg.instr_idx, inmsg.cl);
        else return false;
      endswitch;
      endalias;
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

      for i := 0 to p.CurIdx-1 do
        alias prev: q[i] do
          if (cur.adr = prev.adr) & (prev.access != nt_store) & (!prev.done) then
            return false;
          elsif cur.cst = REL & (!prev.done) then
            return false;
          elsif (prev.cst = ACQ) & (!prev.done) then
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
{{FUNC_BODY_FORBIDDEN}}
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
{{FUNC_BODY_EXPECTED}}
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